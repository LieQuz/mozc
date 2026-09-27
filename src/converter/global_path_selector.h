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

#ifndef MOZC_CONVERTER_GLOBAL_PATH_SELECTOR_H_
#define MOZC_CONVERTER_GLOBAL_PATH_SELECTOR_H_

#include "absl/functional/function_ref.h"
#include "converter/lattice.h"
#include "converter/lexical_transition_bonus.h"
#include "converter/word_ngram.h"
#include "request/options.h"

namespace mozc {

// After POS Viterbi, pick a full-sentence path among those within
// kMaxPosCostGap of the 1-best, using word n-gram bonuses. History nodes stay
// on the POS path. An empty n-gram model leaves the lattice unchanged.
//
// adjusted = pos_cost - ngram_bonus. The POS 1-best wins ties.
void MaybeSelectGlobalPath(
    RequestType request_type,
    absl::FunctionRef<int(uint16_t rid, uint16_t lid)> transition_cost,
    const LexicalTransitionBonus::Snapshot& lexical, const WordNgram& ngram,
    Lattice* lattice);

}  // namespace mozc

#endif  // MOZC_CONVERTER_GLOBAL_PATH_SELECTOR_H_
