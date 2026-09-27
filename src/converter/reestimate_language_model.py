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

"""Reestimate dictionary word costs and the POS connection matrix.

The corpus is one token per line:

  value \\t lid \\t rid

Blank lines separate sentences. Costs use Mozc's -500 * ln(prob) scale.
Entries that never occur keep a high cost for their POS (the 80th percentile
of seen costs), not the median, so unseen words do not tie frequent homophones.

The connection file is the single-column format read by ConnectionFileReader:
a POS-size header, then rid-major costs.

This script does not change C++ types. Replace dictionaryNN.txt costs and
connection_single_column.txt only after evaluation.tsv does not regress.
"""

import collections
import math
import optparse


def _Cost(prob):
  if prob <= 0.0:
    return 0
  return max(0, int(round(-500.0 * math.log(prob))))


def _Percentile(values, fraction):
  if not values:
    return 8000
  ordered = sorted(values)
  index = min(len(ordered) - 1, int(round(fraction * (len(ordered) - 1))))
  return ordered[index]


def LoadCorpus(path):
  word_count = collections.Counter()
  pos_count = collections.Counter()
  transition = collections.Counter()
  rid_count = collections.Counter()
  with open(path, encoding="utf-8") as stream:
    prev_rid = None
    for raw in stream:
      line = raw.strip()
      if not line or line.startswith("#"):
        prev_rid = None
        continue
      columns = line.split("\t")
      if len(columns) < 3:
        continue
      value, lid, rid = columns[0], int(columns[1]), int(columns[2])
      word_count[(value, lid, rid)] += 1
      pos_count[lid] += 1
      if prev_rid is not None:
        transition[(prev_rid, lid)] += 1
        rid_count[prev_rid] += 1
      prev_rid = rid
  return word_count, pos_count, transition, rid_count


def RewriteDictionary(in_path, out_path, word_count, pos_count):
  seen_costs = collections.defaultdict(list)
  rows = []
  with open(in_path, encoding="utf-8") as stream:
    for raw in stream:
      line = raw.rstrip("\n")
      if not line or line.startswith("#"):
        rows.append(line)
        continue
      columns = line.split("\t")
      if len(columns) < 5:
        rows.append(line)
        continue
      key, lid_s, rid_s, _cost, value = columns[:5]
      lid, rid = int(lid_s), int(rid_s)
      count = word_count.get((value, lid, rid), 0)
      total = pos_count.get(lid, 0)
      if count > 0 and total > 0:
        cost = _Cost(count / float(total))
        seen_costs[lid].append(cost)
      else:
        cost = None
      rows.append((key, lid_s, rid_s, cost, value, columns[5:]))

  with open(out_path, "w", encoding="utf-8", newline="\n") as stream:
    for row in rows:
      if isinstance(row, str):
        stream.write(row + "\n")
        continue
      key, lid_s, rid_s, cost, value, rest = row
      if cost is None:
        cost = _Percentile(seen_costs[int(lid_s)], 0.8)
      tail = ("\t" + "\t".join(rest)) if rest else ""
      stream.write("%s\t%s\t%s\t%d\t%s%s\n" %
                   (key, lid_s, rid_s, cost, value, tail))


def WriteConnection(path, pos_size, transition, rid_count):
  with open(path, "w", encoding="utf-8", newline="\n") as stream:
    stream.write("%d\n" % pos_size)
    for rid in range(pos_size):
      total = rid_count.get(rid, 0)
      for lid in range(pos_size):
        count = transition.get((rid, lid), 0)
        if total <= 0 or count <= 0:
          # Unseen POS pair. A large cost keeps it available but unlikely.
          cost = 10000
        else:
          # Add-0.5 smoothing so a single observation is not free.
          cost = _Cost((count + 0.5) / (total + 0.5 * pos_size))
        stream.write("%d\n" % cost)


def main():
  parser = optparse.OptionParser()
  parser.add_option("--corpus", dest="corpus")
  parser.add_option("--dictionary_in", dest="dictionary_in")
  parser.add_option("--dictionary_out", dest="dictionary_out")
  parser.add_option("--connection_out", dest="connection_out")
  parser.add_option("--pos_size", dest="pos_size", type="int", default=2672)
  options, _ = parser.parse_args()
  if not options.corpus or not options.dictionary_in:
    parser.error("--corpus and --dictionary_in are required")
  word_count, pos_count, transition, rid_count = LoadCorpus(options.corpus)
  if options.dictionary_out:
    RewriteDictionary(options.dictionary_in, options.dictionary_out, word_count,
                      pos_count)
  if options.connection_out:
    WriteConnection(options.connection_out, options.pos_size, transition,
                    rid_count)


if __name__ == "__main__":
  main()
