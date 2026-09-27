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

#include "absl/strings/string_view.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(WordNgramTest, EmptyModelScoresZero) {
  WordNgram ngram;
  const absl::string_view words[] = {"東京"};
  EXPECT_TRUE(ngram.empty());
  EXPECT_EQ(ngram.Score(words), 0);
}

TEST(WordNgramTest, BacksOffFromTrigramToUnigram) {
  WordNgram ngram;
  ngram.Load("日本\tの\t首都\t800\n*\t*\t首都\t50\n*\t東京\t都\t200\n");
  const absl::string_view trigram[] = {"日本", "の", "首都"};
  EXPECT_EQ(ngram.Score(trigram), 800);

  const absl::string_view bigram[] = {"東京", "都"};
  EXPECT_EQ(ngram.Score(bigram), 200);

  const absl::string_view unigram[] = {"首都"};
  EXPECT_EQ(ngram.Score(unigram), 50);
}

}  // namespace
}  // namespace mozc
