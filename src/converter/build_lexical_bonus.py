# -*- coding: utf-8 -*-
# Copyright 2010-2021, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Build surface bigram and word trigram bonus tables.

Two inputs are supported.

Collocation phrases (one phrase per line), split on particles:

  python build_lexical_bonus.py --collocation collocation.txt \
      --bigram_out lexical_transition_bonus.tsv \
      --trigram_out word_ngram.tsv

A segmented corpus, one sentence per line, tokens separated by spaces.
Bonuses are 500 * PMI in nats, clamped, and particle-particle pairs are
dropped. Low counts are dropped so function-word pairs do not flood the table.

  python build_lexical_bonus.py --corpus sentences.txt \
      --bigram_out lexical_transition_bonus.tsv \
      --trigram_out word_ngram.tsv
"""

import collections
import math
import optparse

# で and と are particles often enough to keep, but まで and kana-internal と
# (しとう) must stay inside the token.
_PARTICLES = set("をにはがのとへでも")
_BIGRAM_BONUS = 300
_TRIGRAM_BONUS = 200


def _IsHiragana(char):
  return "\u3041" <= char <= "\u3096"


def _IsParticle(token):
  return len(token) == 1 and token in _PARTICLES


def _HasKanjiOrKatakana(token):
  for char in token:
    if "\u4e00" <= char <= "\u9fff":
      return True
    if "\u30a0" <= char <= "\u30ff":
      return True
  return False


def _SplitsAsParticle(buf, char, rest):
  if char not in _PARTICLES:
    return False
  prev_hira = bool(buf) and _IsHiragana(buf[-1])
  next_hira = bool(rest) and _IsHiragana(rest[0])
  # Inside a kana word (にくい, しとう) or at the start of one (もう).
  if prev_hira and next_hira:
    return False
  if not buf and next_hira:
    return False
  # ごはん: は after hiragana is part of the word.
  if char == "は" and prev_hira:
    return False
  # なのか: の after な stays in the word.
  if char == "の" and buf and buf[-1] == "な":
    return False
  # まで stays one token.
  if char == "で" and buf and buf[-1] == "ま":
    return False
  return True


def TokenizePhrase(phrase):
  tokens = []
  buf = []
  text = phrase.strip()
  for index, char in enumerate(text):
    if _SplitsAsParticle(buf, char, text[index + 1:]):
      if buf:
        tokens.append("".join(buf))
        buf = []
      tokens.append(char)
    else:
      buf.append(char)
  if buf:
    tokens.append("".join(buf))
  return tokens


def _AddPairs(tokens, bigrams, trigrams, bigram_bonus, trigram_bonus):
  for left, right in zip(tokens, tokens[1:]):
    if _IsParticle(left) and _IsParticle(right):
      continue
    # A particle followed only by kana (を+する) would bias every sentence.
    if _IsParticle(left) and not _HasKanjiOrKatakana(right):
      continue
    if not left or not right:
      continue
    bigrams[(left, right)] = max(bigrams[(left, right)], bigram_bonus)
  for i in range(len(tokens) - 2):
    triple = tokens[i:i + 3]
    if sum(_IsParticle(token) for token in triple) >= 2:
      continue
    if _IsParticle(triple[1]) and not _HasKanjiOrKatakana(triple[2]):
      continue
    trigrams[tuple(triple)] = max(trigrams[tuple(triple)], trigram_bonus)


def FromCollocation(path):
  bigrams = collections.defaultdict(int)
  trigrams = collections.defaultdict(int)
  with open(path, encoding="utf-8") as stream:
    for line in stream:
      line = line.strip()
      if not line or line.startswith("#"):
        continue
      _AddPairs(TokenizePhrase(line), bigrams, trigrams, _BIGRAM_BONUS,
                _TRIGRAM_BONUS)
  return bigrams, trigrams


def FromCorpus(path, min_count, max_bonus):
  bigram_count = collections.Counter()
  trigram_count = collections.Counter()
  unigram_count = collections.Counter()
  context_count = collections.Counter()
  pair_context = collections.Counter()
  sentences = 0
  with open(path, encoding="utf-8") as stream:
    for line in stream:
      tokens = line.strip().split()
      if len(tokens) < 2 or line.startswith("#"):
        continue
      sentences += 1
      for token in tokens:
        unigram_count[token] += 1
      for left, right in zip(tokens, tokens[1:]):
        if _IsParticle(left) and _IsParticle(right):
          continue
        if _IsParticle(left) and not _HasKanjiOrKatakana(right):
          continue
        bigram_count[(left, right)] += 1
        context_count[left] += 1
      for i in range(len(tokens) - 2):
        triple = tuple(tokens[i:i + 3])
        if sum(_IsParticle(token) for token in triple) >= 2:
          continue
        if _IsParticle(triple[1]) and not _HasKanjiOrKatakana(triple[2]):
          continue
        trigram_count[triple] += 1
        pair_context[(triple[0], triple[1])] += 1

  total = sum(unigram_count.values()) or 1
  bigrams = {}
  for (left, right), count in bigram_count.items():
    if count < min_count:
      continue
    # 500 * PMI, natural log, same scale as -500 * log(prob) in Mozc.
    pmi = math.log(count * total / (context_count[left] * unigram_count[right]))
    bonus = int(round(500.0 * pmi))
    if bonus <= 0:
      continue
    bigrams[(left, right)] = min(max_bonus, bonus)

  trigrams = {}
  for triple, count in trigram_count.items():
    if count < min_count:
      continue
    context = pair_context[(triple[0], triple[1])]
    if context <= 0 or unigram_count[triple[2]] <= 0:
      continue
    pmi = math.log(count * total / (context * unigram_count[triple[2]]))
    bonus = int(round(500.0 * pmi))
    if bonus <= 0:
      continue
    trigrams[triple] = min(max_bonus, bonus)
  return bigrams, trigrams


def _WriteBigrams(path, bigrams):
  with open(path, "w", encoding="utf-8", newline="\n") as stream:
    stream.write("# left\tright\tbonus\n")
    for (left, right), bonus in sorted(bigrams.items()):
      stream.write("%s\t%s\t%d\n" % (left, right, bonus))


def _WriteTrigrams(path, trigrams):
  with open(path, "w", encoding="utf-8", newline="\n") as stream:
    stream.write("# left2\tleft1\tword\tbonus\n")
    for (left2, left1, word), bonus in sorted(trigrams.items()):
      stream.write("%s\t%s\t%s\t%d\n" % (left2, left1, word, bonus))


def main():
  parser = optparse.OptionParser()
  parser.add_option("--collocation", dest="collocation", default="")
  parser.add_option("--corpus", dest="corpus", default="")
  parser.add_option("--bigram_out", dest="bigram_out", default="")
  parser.add_option("--trigram_out", dest="trigram_out", default="")
  parser.add_option("--min_count", dest="min_count", type="int", default=5)
  parser.add_option("--max_bonus", dest="max_bonus", type="int", default=800)
  options, _ = parser.parse_args()
  if not options.bigram_out or not options.trigram_out:
    parser.error("--bigram_out and --trigram_out are required")
  if bool(options.collocation) == bool(options.corpus):
    parser.error("pass exactly one of --collocation or --corpus")
  if options.collocation:
    bigrams, trigrams = FromCollocation(options.collocation)
  else:
    bigrams, trigrams = FromCorpus(options.corpus, options.min_count,
                                   options.max_bonus)
  _WriteBigrams(options.bigram_out, bigrams)
  _WriteTrigrams(options.trigram_out, trigrams)


if __name__ == "__main__":
  main()
