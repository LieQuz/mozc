// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "converter/conversion_layer_eval.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/util.h"
#include "converter/node.h"

namespace mozc {
namespace {

std::vector<size_t> BoundaryPositions(
    absl::Span<const std::string> segments) {
  std::vector<size_t> cuts;
  size_t cursor = 0;
  for (size_t i = 0; i + 1 < segments.size(); ++i) {
    cursor += Util::CharsLen(segments[i]);
    cuts.push_back(cursor);
  }
  return cuts;
}

}  // namespace

absl::Status ParseConversionLayerTsv(absl::string_view tsv,
                                     std::vector<ConversionLayerItem>* items) {
  if (items == nullptr) {
    return absl::InvalidArgumentError("items is null");
  }
  items->clear();
  for (absl::string_view line : absl::StrSplit(tsv, '\n')) {
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    if (line.empty() || line.front() == '#') {
      continue;
    }
    std::vector<absl::string_view> columns = absl::StrSplit(line, '\t');
    if (columns.size() < 3 || columns[0].empty() || columns[1].empty()) {
      return absl::InvalidArgumentError(
          absl::StrCat("expected key, value, segments: ", line));
    }
    ConversionLayerItem item;
    item.key = std::string(columns[0]);
    item.expected_value = std::string(columns[1]);
    for (absl::string_view segment : absl::StrSplit(columns[2], '|')) {
      if (!segment.empty()) {
        item.expected_segments.emplace_back(segment);
      }
    }
    if (columns.size() >= 4) {
      item.domain = std::string(columns[3]);
    }
    if (absl::StrJoin(item.expected_segments, "") != item.expected_value) {
      return absl::InvalidArgumentError(absl::StrCat(
          "segments do not concatenate to the expected value: ", line));
    }
    items->push_back(std::move(item));
  }
  return absl::OkStatus();
}

double SegmentBoundaryF1(absl::Span<const std::string> expected,
                         absl::Span<const std::string> actual) {
  const std::vector<size_t> expected_cuts = BoundaryPositions(expected);
  const std::vector<size_t> actual_cuts = BoundaryPositions(actual);
  if (expected_cuts.empty() && actual_cuts.empty()) {
    return 1.0;
  }
  absl::flat_hash_set<size_t> actual_set(actual_cuts.begin(),
                                         actual_cuts.end());
  int hit = 0;
  for (size_t cut : expected_cuts) {
    if (actual_set.contains(cut)) {
      ++hit;
    }
  }
  const double precision =
      actual_cuts.empty() ? 0.0 : static_cast<double>(hit) / actual_cuts.size();
  const double recall = expected_cuts.empty()
                            ? 0.0
                            : static_cast<double>(hit) / expected_cuts.size();
  if (precision + recall == 0.0) {
    return 0.0;
  }
  return 2.0 * precision * recall / (precision + recall);
}

ConversionLayerResult EvaluateConversionLayer(const ConversionLayerItem& item,
                                              const Lattice& lattice,
                                              const Segments& segments) {
  ConversionLayerResult result;
  absl::flat_hash_set<absl::string_view> node_values;
  if (lattice.has_lattice()) {
    for (size_t pos = 0; pos < lattice.key().size(); ++pos) {
      for (const Node* node : lattice.begin_nodes(pos)) {
        if (node == nullptr || node->node_type == Node::BOS_NODE ||
            node->node_type == Node::EOS_NODE) {
          continue;
        }
        node_values.insert(node->value);
      }
    }
  }
  result.in_lattice = true;
  for (const std::string& segment : item.expected_segments) {
    if (!node_values.contains(segment)) {
      result.in_lattice = false;
      break;
    }
  }

  std::vector<std::string> actual_segments;
  int worst_rank = 0;
  bool rank_known = true;
  for (const Segment& segment : segments.conversion_segments()) {
    if (segment.candidates_size() == 0) {
      continue;
    }
    actual_segments.emplace_back(segment.candidate(0).value);
  }
  result.best_value = absl::StrJoin(actual_segments, "");
  result.best_path_match = result.best_value == item.expected_value;
  result.boundaries_match =
      SegmentBoundaryF1(item.expected_segments, actual_segments) == 1.0 &&
      actual_segments.size() == item.expected_segments.size();
  result.boundary_f1 =
      SegmentBoundaryF1(item.expected_segments, actual_segments);

  if (result.boundaries_match) {
    for (size_t i = 0; i < item.expected_segments.size(); ++i) {
      const Segment& segment = segments.conversion_segment(i);
      int rank = -1;
      for (size_t c = 0; c < segment.candidates_size(); ++c) {
        if (segment.candidate(c).value == item.expected_segments[i]) {
          rank = static_cast<int>(c);
          break;
        }
      }
      if (rank < 0) {
        rank_known = false;
        worst_rank = -1;
        break;
      }
      worst_rank = std::max(worst_rank, rank);
    }
  }
  result.candidate_rank = rank_known ? worst_rank : -1;

  if (!result.in_lattice) {
    result.layer = ConversionFailureLayer::kOov;
  } else if (!result.boundaries_match) {
    result.layer = ConversionFailureLayer::kBestPath;
  } else if (result.candidate_rank != 0) {
    result.layer = ConversionFailureLayer::kCandidateRank;
  } else {
    result.layer = ConversionFailureLayer::kOk;
  }
  return result;
}

std::string ConversionFailureLayerName(ConversionFailureLayer layer) {
  switch (layer) {
    case ConversionFailureLayer::kOk:
      return "ok";
    case ConversionFailureLayer::kOov:
      return "oov";
    case ConversionFailureLayer::kBestPath:
      return "best_path";
    case ConversionFailureLayer::kCandidateRank:
      return "candidate_rank";
  }
  return "unknown";
}

}  // namespace mozc
