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

#ifndef MOZC_CONVERTER_CONVERSION_LAYER_EVAL_H_
#define MOZC_CONVERTER_CONVERSION_LAYER_EVAL_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/lattice.h"
#include "converter/segments.h"

namespace mozc {

// Where a gold sentence was lost. The layers match
// docs/design_doc/conversion_accuracy.md.
enum class ConversionFailureLayer {
  kOk = 0,
  // An expected word is not a node in the lattice.
  kOov = 1,
  // The word is in the lattice, but the 1-best boundaries differ.
  kBestPath = 2,
  // Boundaries match and the expected word is only a lower candidate.
  kCandidateRank = 3,
};

struct ConversionLayerItem {
  std::string key;
  std::string expected_value;
  // Gold word surfaces. Their concatenation is `expected_value`.
  std::vector<std::string> expected_segments;
  std::string domain;
};

struct ConversionLayerResult {
  ConversionFailureLayer layer = ConversionFailureLayer::kOk;
  bool in_lattice = false;
  bool best_path_match = false;
  bool boundaries_match = false;
  // 0 when the expected surface is the top candidate of its segment.
  // -1 when it is absent from that segment.
  int candidate_rank = -1;
  double boundary_f1 = 0;
  std::string best_value;
};

// TSV columns: key, expected_value, expected_segments, domain.
// expected_segments is '|' separated. Lines starting with '#' are skipped.
absl::Status ParseConversionLayerTsv(absl::string_view tsv,
                                     std::vector<ConversionLayerItem>* items);

// Character-boundary F1 between two segmentations of the same sentence.
double SegmentBoundaryF1(absl::Span<const std::string> expected,
                         absl::Span<const std::string> actual);

// `segments` are the immutable converter's conversion segments. History
// segments are ignored. `lattice` is the lattice from the same Convert call.
ConversionLayerResult EvaluateConversionLayer(const ConversionLayerItem& item,
                                              const Lattice& lattice,
                                              const Segments& segments);

std::string ConversionFailureLayerName(ConversionFailureLayer layer);

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERSION_LAYER_EVAL_H_
