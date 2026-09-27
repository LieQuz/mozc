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

#include "converter/lexical_transition_bonus.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/japanese_util.h"

namespace mozc {
namespace {

bool ContainsKatakana(absl::string_view text) {
  const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
  for (size_t i = 0; i + 2 < text.size(); ++i) {
    if (bytes[i] == 0xE3 && bytes[i + 1] == 0x83) {
      return true;
    }
    if (bytes[i] == 0xE3 && bytes[i + 1] == 0x82 && bytes[i + 2] >= 0xA0) {
      return true;
    }
    // Half-width katakana is U+FF61..U+FF9F.
    if (bytes[i] == 0xEF && bytes[i + 1] == 0xBD && bytes[i + 2] >= 0xA1) {
      return true;
    }
    if (bytes[i] == 0xEF && bytes[i + 1] == 0xBE && bytes[i + 2] <= 0x9F) {
      return true;
    }
  }
  return false;
}

std::string NormalizeSurface(absl::string_view text) {
  if (!ContainsKatakana(text)) {
    return std::string(text);
  }
  const std::string fullwidth =
      japanese_util::HalfWidthKatakanaToFullWidthKatakana(text);
  return japanese_util::KatakanaToHiragana(fullwidth);
}

// Returns `text` when it is already in lookup form, otherwise fills `buf`.
absl::string_view Canonical(absl::string_view text, std::string* buf) {
  if (!ContainsKatakana(text)) {
    return text;
  }
  *buf = NormalizeSurface(text);
  return *buf;
}

int FindBonus(const LexicalTransitionBonus::SurfaceMap& map,
              absl::string_view left, absl::string_view right) {
  const auto outer = map.find(left);
  if (outer == map.end()) {
    return 0;
  }
  const auto inner = outer->second.find(right);
  if (inner == outer->second.end()) {
    return 0;
  }
  return inner->second;
}

int BonusFromCount(int count, int cap) {
  if (count <= 0 || cap <= 0) {
    return 0;
  }
  int bits = 0;
  int n = count + 1;
  while (n > 1) {
    n >>= 1;
    ++bits;
  }
  return std::min(cap, LexicalTransitionBonus::kUserBonusUnit * bits);
}

void AddCount(LexicalTransitionBonus::SurfaceMap* map, absl::string_view left,
              absl::string_view right, int delta) {
  auto& inner = (*map)[std::string(left)];
  int& count = inner[std::string(right)];
  count += delta;
  if (count <= 0) {
    inner.erase(inner.find(right));
    if (inner.empty()) {
      map->erase(map->find(left));
    }
  }
}

}  // namespace

size_t LexicalTransitionBonus::TransparentStringHash::operator()(
    absl::string_view value) const {
  return absl::HashOf(value);
}

int LexicalTransitionBonus::Snapshot::Lookup(absl::string_view left,
                                             absl::string_view right) const {
  if ((static_bonus_ == nullptr || static_bonus_->empty()) &&
      (user_counts_ == nullptr || user_counts_->empty())) {
    return 0;
  }
  std::string left_buf;
  std::string right_buf;
  const absl::string_view canonical_left = Canonical(left, &left_buf);
  const absl::string_view canonical_right = Canonical(right, &right_buf);
  int bonus = 0;
  if (static_bonus_ != nullptr) {
    bonus = FindBonus(*static_bonus_, canonical_left, canonical_right);
  }
  if (user_counts_ != nullptr) {
    const int count =
        FindBonus(*user_counts_, canonical_left, canonical_right);
    bonus = std::max(bonus, BonusFromCount(count, user_cap_));
  }
  return bonus;
}

int LexicalTransitionBonus::Snapshot::Apply(int pos_cost,
                                            absl::string_view left,
                                            absl::string_view right) const {
  if (pos_cost <= 0 || empty()) {
    return pos_cost;
  }
  const int bonus = Lookup(left, right);
  if (bonus <= 0) {
    return pos_cost;
  }
  return std::max(0, pos_cost - bonus);
}

bool LexicalTransitionBonus::Snapshot::empty() const {
  const bool no_static = static_bonus_ == nullptr || static_bonus_->empty();
  const bool no_user = user_counts_ == nullptr || user_counts_->empty();
  return no_static && no_user;
}

void LexicalTransitionBonus::LoadStatic(absl::string_view tsv) {
  std::lock_guard<std::mutex> lock(mutex_);
  static_bonus_.clear();
  user_cap_ = kUserBonusCap;
  for (absl::string_view line : absl::StrSplit(tsv, '\n')) {
    if (line.empty() || line.front() == '#') {
      continue;
    }
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    std::vector<absl::string_view> columns = absl::StrSplit(line, '\t');
    if (columns.size() < 3 || columns[0].empty() || columns[1].empty()) {
      continue;
    }
    int bonus = 0;
    if (!absl::SimpleAtoi(columns[2], &bonus) || bonus <= 0) {
      continue;
    }
    const std::string left = NormalizeSurface(columns[0]);
    const std::string right = NormalizeSurface(columns[1]);
    int& slot = static_bonus_[left][right];
    slot = std::max(slot, bonus);
    user_cap_ = std::max(user_cap_, bonus);
  }
}

LexicalTransitionBonus::Snapshot LexicalTransitionBonus::GetSnapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  Snapshot snapshot;
  snapshot.static_bonus_ = &static_bonus_;
  snapshot.user_counts_ = user_counts_;
  snapshot.user_cap_ = user_cap_;
  return snapshot;
}

void LexicalTransitionBonus::NoteCommittedSequence(
    uint32_t revert_id, absl::Span<const absl::string_view> values) {
  std::vector<std::pair<std::string, std::string>> pairs;
  std::string previous;
  bool has_previous = false;
  for (absl::string_view value : values) {
    std::string current = NormalizeSurface(value);
    if (current.empty()) {
      continue;
    }
    if (has_previous) {
      pairs.emplace_back(previous, current);
    }
    previous = std::move(current);
    has_previous = true;
  }
  if (pairs.empty()) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  SurfaceMap counts = user_counts_ ? *user_counts_ : SurfaceMap();
  const auto existing = revert_pairs_.find(revert_id);
  if (existing != revert_pairs_.end()) {
    for (const auto& [left, right] : existing->second) {
      AddCount(&counts, left, right, -1);
    }
  }
  for (const auto& [left, right] : pairs) {
    AddCount(&counts, left, right, +1);
  }
  revert_pairs_[revert_id] = std::move(pairs);
  user_counts_ = std::make_shared<const SurfaceMap>(std::move(counts));
}

void LexicalTransitionBonus::Revert(uint32_t revert_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto existing = revert_pairs_.find(revert_id);
  if (existing == revert_pairs_.end()) {
    return;
  }
  SurfaceMap counts = user_counts_ ? *user_counts_ : SurfaceMap();
  for (const auto& [left, right] : existing->second) {
    AddCount(&counts, left, right, -1);
  }
  revert_pairs_.erase(existing);
  user_counts_ = std::make_shared<const SurfaceMap>(std::move(counts));
}

void LexicalTransitionBonus::ClearUserPairs() {
  std::lock_guard<std::mutex> lock(mutex_);
  user_counts_ = std::make_shared<const SurfaceMap>();
  revert_pairs_.clear();
}

}  // namespace mozc
