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

#include "converter/global_path_selector.h"

#include <string>

#include "absl/strings/string_view.h"
#include "converter/lattice.h"
#include "converter/lexical_transition_bonus.h"
#include "converter/node.h"
#include "converter/word_ngram.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

Node* AddNode(Lattice* lattice, size_t pos, absl::string_view key,
              absl::string_view value, int wcost) {
  Node* node = lattice->NewNode();
  node->key = std::string(key);
  node->value = std::string(value);
  node->wcost = wcost;
  node->lid = 1;
  node->rid = 1;
  node->node_type = Node::NOR_NODE;
  lattice->Insert(pos, node);
  return node;
}

TEST(GlobalPathSelectorTest, EmptyNgramKeepsPosPath) {
  Lattice lattice;
  lattice.SetKey("ab");
  Node* left = AddNode(&lattice, 0, "a", "左", 100);
  Node* right = AddNode(&lattice, 1, "b", "右", 100);
  Node* bos = lattice.bos_node();
  Node* eos = lattice.eos_node();
  left->prev = bos;
  left->cost = 110;
  bos->next = left;
  right->prev = left;
  right->cost = 220;
  left->next = right;
  eos->prev = right;
  eos->cost = 230;
  right->next = eos;

  WordNgram ngram;
  LexicalTransitionBonus bonus;
  MaybeSelectGlobalPath(
      RequestType::CONVERSION, [](uint16_t, uint16_t) { return 10; },
      bonus.GetSnapshot(), ngram, &lattice);
  EXPECT_EQ(eos->prev, right);
}

TEST(GlobalPathSelectorTest, TrigramSelectsNearAlternative) {
  Lattice lattice;
  lattice.SetKey("ab");
  Node* left = AddNode(&lattice, 0, "a", "左", 100);
  Node* right = AddNode(&lattice, 1, "b", "右", 100);
  Node* whole = AddNode(&lattice, 0, "ab", "全体", 250);
  Node* bos = lattice.bos_node();
  Node* eos = lattice.eos_node();
  left->prev = bos;
  left->cost = 110;
  bos->next = left;
  right->prev = left;
  right->cost = 220;
  left->next = right;
  eos->prev = right;
  eos->cost = 230;
  right->next = eos;

  WordNgram ngram;
  ngram.Load("*\t*\t全体\t100\n");
  LexicalTransitionBonus bonus;
  MaybeSelectGlobalPath(
      RequestType::CONVERSION, [](uint16_t, uint16_t) { return 10; },
      bonus.GetSnapshot(), ngram, &lattice);
  EXPECT_EQ(eos->prev, whole);
  EXPECT_EQ(whole->prev, bos);
}

}  // namespace
}  // namespace mozc
