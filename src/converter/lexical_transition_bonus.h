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

#ifndef MOZC_CONVERTER_LEXICAL_TRANSITION_BONUS_H_
#define MOZC_CONVERTER_LEXICAL_TRANSITION_BONUS_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc {

// Sparse surface bigram subtracted from the POS transition cost.
//
// The POS connector stays the default. A known pair (left value, right value)
// reduces that transition, and the result is clamped at 0. Unknown pairs leave
// the POS cost unchanged.
//
// Static pairs come from a TSV. User pairs are added only when a candidate is
// committed, grow with log2(count), and are capped at the corpus-scale bonus
// so one mistaken commit cannot override the system table.
class LexicalTransitionBonus {
 public:
  // First-commit bonus. Two commits are still in this band; the cap is reached
  // only after repeated selection of the same pair.
  static constexpr int kUserBonusUnit = 200;
  // Floor of the user-bonus cap. LoadStatic raises it to the largest static
  // bonus so user learning can match, and not exceed, corpus evidence.
  static constexpr int kUserBonusCap = 300;

  struct TransparentStringHash {
    using is_transparent = void;
    size_t operator()(absl::string_view value) const;
  };
  struct TransparentStringEq {
    using is_transparent = void;
    bool operator()(absl::string_view lhs, absl::string_view rhs) const {
      return lhs == rhs;
    }
  };

  using InnerMap =
      absl::flat_hash_map<std::string, int, TransparentStringHash,
                          TransparentStringEq>;
  using SurfaceMap =
      absl::flat_hash_map<std::string, InnerMap, TransparentStringHash,
                          TransparentStringEq>;

  // Immutable view used for the duration of one Viterbi / n-best search.
  // Copying a snapshot does not lock on each edge.
  class Snapshot {
   public:
    Snapshot() = default;

    int Lookup(absl::string_view left, absl::string_view right) const;
    // POS transition after the surface bonus. Non-positive costs are kept.
    int Apply(int pos_cost, absl::string_view left,
              absl::string_view right) const;
    bool empty() const;

   private:
    friend class LexicalTransitionBonus;
    const SurfaceMap* static_bonus_ = nullptr;
    std::shared_ptr<const SurfaceMap> user_counts_;
    int user_cap_ = kUserBonusCap;
  };

  LexicalTransitionBonus() = default;

  // TSV lines: left \t right \t bonus. '#' comments and blank lines are
  // ignored. Bonus must be positive. Katakana is stored as hiragana.
  void LoadStatic(absl::string_view tsv);

  Snapshot GetSnapshot() const;

  int Lookup(absl::string_view left, absl::string_view right) const {
    return GetSnapshot().Lookup(left, right);
  }
  int Apply(int pos_cost, absl::string_view left,
            absl::string_view right) const {
    return GetSnapshot().Apply(pos_cost, left, right);
  }
  bool HasEntries() const { return !GetSnapshot().empty(); }

  // `values` are consecutive committed surfaces, oldest first. The first
  // value may be the previous context. Pairs are counted once per call.
  void NoteCommittedSequence(uint32_t revert_id,
                             absl::Span<const absl::string_view> values);
  void Revert(uint32_t revert_id);
  void ClearUserPairs();

 private:
  int UserBonus(int count) const;

  SurfaceMap static_bonus_;
  std::shared_ptr<const SurfaceMap> user_counts_ =
      std::make_shared<const SurfaceMap>();
  int user_cap_ = kUserBonusCap;
  absl::flat_hash_map<uint32_t, std::vector<std::pair<std::string, std::string>>>
      revert_pairs_;
  mutable std::mutex mutex_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_LEXICAL_TRANSITION_BONUS_H_
