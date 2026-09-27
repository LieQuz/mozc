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

#include <algorithm>
#include <cstddef>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/node.h"

namespace mozc {
namespace {

constexpr int kBeamWidth = 32;
// Same window CollocationRewriter uses for candidate promotion:
// -500 * log(1/1000).
constexpr int kMaxPosCostGap = 3453;

struct State {
  Node* node = nullptr;
  int cost = 0;
  int parent = -1;
};

struct Candidate {
  int pos_cost = 0;
  int adjusted = 0;
  std::vector<Node*> nodes;
};

std::vector<absl::string_view> ValuesOf(const std::vector<Node*>& nodes) {
  std::vector<absl::string_view> values;
  values.reserve(nodes.size());
  for (const Node* node : nodes) {
    values.emplace_back(node->value);
  }
  return values;
}

bool SameNodes(const std::vector<Node*>& lhs, const std::vector<Node*>& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i] != rhs[i]) {
      return false;
    }
  }
  return true;
}

std::vector<Node*> Reconstruct(absl::Span<const State> states, int eos_index,
                               const Node* anchor) {
  std::vector<Node*> nodes;
  for (int index = states[eos_index].parent; index >= 0;
       index = states[index].parent) {
    Node* node = states[index].node;
    if (node == nullptr || node == anchor ||
        node->node_type == Node::BOS_NODE ||
        node->node_type == Node::HIS_NODE ||
        node->node_type == Node::EOS_NODE) {
      break;
    }
    nodes.push_back(node);
  }
  std::reverse(nodes.begin(), nodes.end());
  return nodes;
}

void TruncateBeam(std::vector<int>* ids, absl::Span<const State> states,
                  const absl::flat_hash_set<const Node*>& keep) {
  if (static_cast<int>(ids->size()) <= kBeamWidth) {
    return;
  }
  std::stable_sort(ids->begin(), ids->end(), [&](int lhs, int rhs) {
    return states[lhs].cost < states[rhs].cost;
  });
  std::vector<int> kept;
  kept.reserve(kBeamWidth);
  for (int id : *ids) {
    if (keep.contains(states[id].node)) {
      kept.push_back(id);
    }
  }
  for (int id : *ids) {
    if (static_cast<int>(kept.size()) >= kBeamWidth) {
      break;
    }
    if (!keep.contains(states[id].node)) {
      kept.push_back(id);
    }
  }
  if (static_cast<int>(kept.size()) > kBeamWidth) {
    kept.resize(kBeamWidth);
  }
  ids->swap(kept);
}

void Rewire(Node* anchor, Node* eos, const std::vector<Node*>& nodes,
            absl::FunctionRef<int(uint16_t, uint16_t)> transition_cost,
            const LexicalTransitionBonus::Snapshot& lexical) {
  Node* prev = anchor;
  int running = anchor->cost;
  for (Node* node : nodes) {
    const int transition = lexical.Apply(transition_cost(prev->rid, node->lid),
                                         prev->value, node->value);
    running += transition + node->wcost;
    node->cost = running;
    node->prev = prev;
    prev->next = node;
    prev = node;
  }
  const int transition = lexical.Apply(transition_cost(prev->rid, eos->lid),
                                       prev->value, eos->value);
  eos->cost = running + transition + eos->wcost;
  eos->prev = prev;
  prev->next = eos;
}

}  // namespace

void MaybeSelectGlobalPath(
    RequestType request_type,
    absl::FunctionRef<int(uint16_t rid, uint16_t lid)> transition_cost,
    const LexicalTransitionBonus::Snapshot& lexical, const WordNgram& ngram,
    Lattice* lattice) {
  if (request_type != RequestType::CONVERSION || lattice == nullptr ||
      ngram.empty() || !lattice->has_lattice()) {
    return;
  }
  Node* bos = lattice->bos_node();
  Node* eos = lattice->eos_node();
  if (bos == nullptr || eos == nullptr || eos->prev == nullptr) {
    return;
  }

  Node* anchor = bos;
  for (Node* node = bos->next;
       node != nullptr && node != eos && node->node_type == Node::HIS_NODE;
       node = node->next) {
    anchor = node;
  }

  std::vector<Node*> viterbi_nodes;
  for (Node* node = anchor->next; node != nullptr && node != eos;
       node = node->next) {
    viterbi_nodes.push_back(node);
  }
  if (viterbi_nodes.empty()) {
    return;
  }

  const int best_pos_cost = eos->cost;
  const int cost_limit = best_pos_cost + kMaxPosCostGap;
  const size_t origin = anchor->end_pos;
  if (origin > lattice->key().size()) {
    return;
  }

  absl::flat_hash_set<const Node*> keep(viterbi_nodes.begin(),
                                        viterbi_nodes.end());
  keep.insert(eos);

  std::vector<State> states;
  states.push_back(State{anchor, anchor->cost, -1});
  std::vector<std::vector<int>> beam(lattice->key().size() + 1);
  beam[origin].push_back(0);

  for (size_t pos = origin; pos < beam.size(); ++pos) {
    TruncateBeam(&beam[pos], states, keep);
    const std::vector<int> frontier = beam[pos];
    for (int index : frontier) {
      Node* left = states[index].node;
      if (left == nullptr || left->node_type == Node::EOS_NODE) {
        continue;
      }
      for (Node* right : lattice->begin_nodes(pos)) {
        if (right == nullptr || right->node_type == Node::BOS_NODE ||
            right->begin_pos < origin) {
          continue;
        }
        const int transition = lexical.Apply(
            transition_cost(left->rid, right->lid), left->value, right->value);
        const int cost = states[index].cost + transition + right->wcost;
        if (right->end_pos >= beam.size()) {
          continue;
        }
        states.push_back(State{right, cost, index});
        beam[right->end_pos].push_back(static_cast<int>(states.size() - 1));
      }
    }
  }

  const int viterbi_adjusted =
      best_pos_cost - ngram.Score(ValuesOf(viterbi_nodes));
  Candidate best;
  best.pos_cost = best_pos_cost;
  best.adjusted = viterbi_adjusted;
  best.nodes = viterbi_nodes;

  if (lattice->key().size() < beam.size()) {
    for (int index : beam[lattice->key().size()]) {
      if (states[index].node == nullptr ||
          states[index].node->node_type != Node::EOS_NODE) {
        continue;
      }
      if (states[index].cost > cost_limit) {
        continue;
      }
      std::vector<Node*> nodes = Reconstruct(states, index, anchor);
      if (nodes.empty()) {
        continue;
      }
      const int adjusted = states[index].cost - ngram.Score(ValuesOf(nodes));
      if (adjusted < best.adjusted ||
          (adjusted == best.adjusted && states[index].cost < best.pos_cost)) {
        best.pos_cost = states[index].cost;
        best.adjusted = adjusted;
        best.nodes = std::move(nodes);
      }
    }
  }

  if (SameNodes(best.nodes, viterbi_nodes)) {
    return;
  }
  Rewire(anchor, eos, best.nodes, transition_cost, lexical);
}

}  // namespace mozc
