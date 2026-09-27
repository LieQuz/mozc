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

#include "converter/word_ngram.h"

#include <algorithm>
#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/japanese_util.h"

namespace mozc {
namespace {

constexpr absl::string_view kAny = "*";

std::string Normalize(absl::string_view text) {
  if (text == kAny || text.empty()) {
    return std::string(kAny);
  }
  const std::string fullwidth =
      japanese_util::HalfWidthKatakanaToFullWidthKatakana(text);
  return japanese_util::KatakanaToHiragana(fullwidth);
}

}  // namespace

void WordNgram::Load(absl::string_view tsv) {
  trigram_.clear();
  for (absl::string_view line : absl::StrSplit(tsv, '\n')) {
    if (line.empty() || line.front() == '#') {
      continue;
    }
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    std::vector<absl::string_view> columns = absl::StrSplit(line, '\t');
    if (columns.size() < 4 || columns[2].empty()) {
      continue;
    }
    int bonus = 0;
    if (!absl::SimpleAtoi(columns[3], &bonus) || bonus <= 0) {
      continue;
    }
    const std::string left2 = Normalize(columns[0]);
    const std::string left1 = Normalize(columns[1]);
    const std::string word = Normalize(columns[2]);
    // Pack (left1, word) into the inner key so a two-level map holds a trigram.
    const std::string packed = absl::StrCat(left1, "\t", word);
    int& slot = trigram_[left2][packed];
    slot = std::max(slot, bonus);
  }
}

int WordNgram::Score(absl::Span<const absl::string_view> words) const {
  if (trigram_.empty() || words.empty()) {
    return 0;
  }
  int total = 0;
  std::string prev2_buf(kAny);
  std::string prev1_buf(kAny);
  for (absl::string_view word : words) {
    const std::string normalized = Normalize(word);
    if (normalized.empty() || normalized == kAny) {
      continue;
    }
    const auto lookup = [&](absl::string_view left2,
                            absl::string_view left1) -> int {
      const auto outer = trigram_.find(left2);
      if (outer == trigram_.end()) {
        return 0;
      }
      const std::string packed = absl::StrCat(left1, "\t", normalized);
      const auto inner = outer->second.find(packed);
      if (inner == outer->second.end()) {
        return 0;
      }
      return inner->second;
    };
    int bonus = lookup(prev2_buf, prev1_buf);
    if (bonus == 0 && !(prev2_buf == kAny && prev1_buf == kAny)) {
      bonus = lookup(kAny, prev1_buf);
    }
    if (bonus == 0 && prev1_buf != kAny) {
      bonus = lookup(kAny, kAny);
    }
    total += bonus;
    prev2_buf = prev1_buf;
    prev1_buf = normalized;
  }
  return total;
}

}  // namespace mozc
