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

#ifndef MOZC_CONVERTER_WORD_NGRAM_H_
#define MOZC_CONVERTER_WORD_NGRAM_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/lexical_transition_bonus.h"

namespace mozc {

// Sparse word n-gram bonuses in the same cost units as Viterbi.
// A path score is the sum of the most specific hit at each word: trigram, then
// bigram, then unigram. An empty model scores every path as 0.
//
// TSV columns: left2 \t left1 \t word \t bonus.
// Use "*" for a missing context word.
class WordNgram {
 public:
  using Map = LexicalTransitionBonus::SurfaceMap;

  void Load(absl::string_view tsv);
  bool empty() const { return trigram_.empty(); }

  int Score(absl::Span<const absl::string_view> words) const;

 private:
  // trigram_[left2][left1][word] = bonus. Empty context is stored as "*".
  Map trigram_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_WORD_NGRAM_H_
