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

"""Merge Mozc UT dictionaries into one system-dictionary file.

Reads bz2 files from https://github.com/utuhiro78/mozcdic-ut-* and writes
Mozc dictionary lines. Readings already in dictionary0*.txt are skipped.
Every added word uses the 名詞,一般 id from id.def, which is how
merge-ut-dictionaries attaches UT entries to the current Mozc POS set.
Costs shipped in the UT files are kept.
"""

import argparse
import bz2
import pathlib
from unicodedata import normalize


def GeneralNounId(id_def):
  with open(id_def, encoding='utf-8') as file:
    for line in file:
      if ' 名詞,一般,' in line:
        return line.split(' ', 1)[0]
  raise ValueError('名詞,一般 was not found in id.def')


def NormalizeSurface(text):
  return normalize('NFKC', text).replace('~', '〜')


def UsableSurface(text):
  length = len(text)
  return 2 <= length <= 25 and '\t' not in text and '\n' not in text


def LoadExisting(dictionary_txts):
  existing = set()
  for path in dictionary_txts:
    with open(path, encoding='utf-8') as file:
      for line in file:
        if not line or line.startswith('#'):
          continue
        columns = line.rstrip('\n').split('\t')
        if len(columns) < 5:
          continue
        existing.add((columns[0], NormalizeSurface(columns[4])))
  return existing


def ReadUtEntries(path):
  opener = bz2.open if str(path).endswith('.bz2') else open
  with opener(path, 'rt', encoding='utf-8') as file:
    for line in file:
      columns = line.rstrip('\n').split('\t')
      if len(columns) < 5:
        continue
      reading, _lid, _rid, cost, surface = columns[:5]
      surface = NormalizeSurface(surface)
      if not reading or not UsableSurface(surface):
        continue
      try:
        cost_value = int(cost)
      except ValueError:
        continue
      if cost_value < 0 or cost_value > 32767:
        continue
      yield reading, surface, cost_value


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--id_def', required=True)
  parser.add_argument('--dictionary_txts', nargs='+', required=True)
  parser.add_argument('--ut_files', nargs='+', required=True)
  parser.add_argument('--output', required=True)
  args = parser.parse_args()

  noun_id = GeneralNounId(args.id_def)
  existing = LoadExisting(args.dictionary_txts)
  best_cost = {}
  for ut_file in args.ut_files:
    for reading, surface, cost in ReadUtEntries(ut_file):
      key = (reading, surface)
      if key in existing:
        continue
      previous = best_cost.get(key)
      if previous is None or cost < previous:
        best_cost[key] = cost

  output = pathlib.Path(args.output)
  output.parent.mkdir(parents=True, exist_ok=True)
  with output.open('w', encoding='utf-8', newline='\n') as file:
    for (reading, surface), cost in sorted(best_cost.items()):
      file.write(f'{reading}\t{noun_id}\t{noun_id}\t{cost}\t{surface}\n')
  print(f'wrote {len(best_cost)} entries to {output}')


if __name__ == '__main__':
  main()
