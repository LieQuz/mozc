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

#include <string>
#include <vector>

#include "converter/lattice.h"
#include "converter/node.h"
#include "converter/segments.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

void AddWord(Lattice* lattice, size_t pos, absl::string_view key,
             absl::string_view value) {
  Node* node = lattice->NewNode();
  node->key = std::string(key);
  node->value = std::string(value);
  node->node_type = Node::NOR_NODE;
  lattice->Insert(pos, node);
}

Segment* AddSegment(Segments* segments, absl::string_view key,
                    absl::string_view top, absl::string_view second) {
  Segment* segment = segments->add_segment();
  segment->set_key(key);
  segment->add_candidate()->value = std::string(top);
  if (!second.empty()) {
    segment->add_candidate()->value = std::string(second);
  }
  return segment;
}

TEST(ConversionLayerEvalTest, BoundaryF1) {
  const std::vector<std::string> expected = {"折れ", "かかった"};
  const std::vector<std::string> same = {"折れ", "かかった"};
  const std::vector<std::string> other = {"折れかかった"};
  EXPECT_DOUBLE_EQ(SegmentBoundaryF1(expected, same), 1.0);
  EXPECT_DOUBLE_EQ(SegmentBoundaryF1(expected, other), 0.0);
}

TEST(ConversionLayerEvalTest, ClassifiesOovBestPathAndRank) {
  std::vector<ConversionLayerItem> items;
  ASSERT_OK(ParseConversionLayerTsv(
      "あ\t亜\t亜\tgeneral\n"
      "ab\t左右\t左|右\tgeneral\n"
      "ab\t左右\t左|右\trank\n",
      &items));
  ASSERT_EQ(items.size(), 3);

  Lattice missing;
  missing.SetKey("あ");
  Segments missing_segments;
  AddSegment(&missing_segments, "あ", "阿", "");
  EXPECT_EQ(EvaluateConversionLayer(items[0], missing, missing_segments).layer,
            ConversionFailureLayer::kOov);

  Lattice lattice;
  lattice.SetKey("ab");
  AddWord(&lattice, 0, "a", "左");
  AddWord(&lattice, 1, "b", "右");
  AddWord(&lattice, 0, "ab", "左右");
  Segments split;
  AddSegment(&split, "a", "左", "");
  AddSegment(&split, "b", "右", "");
  EXPECT_EQ(EvaluateConversionLayer(items[1], lattice, split).layer,
            ConversionFailureLayer::kOk);

  Segments ranked;
  AddSegment(&ranked, "a", "佐", "左");
  AddSegment(&ranked, "b", "右", "");
  const ConversionLayerResult rank =
      EvaluateConversionLayer(items[2], lattice, ranked);
  EXPECT_EQ(rank.layer, ConversionFailureLayer::kCandidateRank);
  EXPECT_EQ(rank.candidate_rank, 1);
}

}  // namespace
}  // namespace mozc
