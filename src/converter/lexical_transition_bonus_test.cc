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

#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(LexicalTransitionBonusTest, UnknownPairLeavesPosCost) {
  LexicalTransitionBonus bonus;
  EXPECT_TRUE(bonus.GetSnapshot().empty());
  EXPECT_EQ(bonus.Apply(1200, "東京", "都"), 1200);
}

TEST(LexicalTransitionBonusTest, StaticBonusIsClampedAtZero) {
  LexicalTransitionBonus bonus;
  bonus.LoadStatic("東京\t都\t500\n# comment\n\n悪い\t行\t0\n");
  EXPECT_EQ(bonus.Lookup("東京", "都"), 500);
  EXPECT_EQ(bonus.Apply(1200, "東京", "都"), 700);
  EXPECT_EQ(bonus.Apply(100, "東京", "都"), 0);
  EXPECT_EQ(bonus.Lookup("悪い", "行"), 0);
}

TEST(LexicalTransitionBonusTest, KatakanaMatchesHiraganaKey) {
  LexicalTransitionBonus bonus;
  bonus.LoadStatic("まぐろ\tを\t300\n");
  EXPECT_EQ(bonus.Lookup("マグロ", "を"), 300);
}

TEST(LexicalTransitionBonusTest, UserCommitGrowsThenReverts) {
  LexicalTransitionBonus bonus;
  const absl::string_view values[] = {"東京", "都"};
  bonus.NoteCommittedSequence(1, values);
  EXPECT_EQ(bonus.Lookup("東京", "都"), LexicalTransitionBonus::kUserBonusUnit);

  bonus.NoteCommittedSequence(2, values);
  bonus.NoteCommittedSequence(3, values);
  EXPECT_GT(bonus.Lookup("東京", "都"), LexicalTransitionBonus::kUserBonusUnit);
  EXPECT_LE(bonus.Lookup("東京", "都"), LexicalTransitionBonus::kUserBonusCap);

  bonus.Revert(3);
  bonus.Revert(2);
  EXPECT_EQ(bonus.Lookup("東京", "都"), LexicalTransitionBonus::kUserBonusUnit);
  bonus.ClearUserPairs();
  EXPECT_EQ(bonus.Lookup("東京", "都"), 0);
}

TEST(LexicalTransitionBonusTest, UserBonusDoesNotExceedStaticCap) {
  LexicalTransitionBonus bonus;
  bonus.LoadStatic("東京\t都\t300\n");
  const absl::string_view values[] = {"東京", "都"};
  for (uint32_t i = 1; i <= 20; ++i) {
    bonus.NoteCommittedSequence(i, values);
  }
  EXPECT_EQ(bonus.Lookup("東京", "都"), 300);
}

}  // namespace
}  // namespace mozc
