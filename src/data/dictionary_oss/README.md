# data/dictionary_oss

## aux_dictionary.tsv

AUX dictionary is a mechanism to add word entries to
`data/dictionary_oss/dictionary0x.txt` without running the internal training
pipeline.

> [!NOTE]
>
> We recommend to update TSV files in
> [data/dictionary_manual](https://github.com/google/mozc/blob/master/src/data/dictionary_manual/) as an
> easier way. Please consider using `data/dictionary_manual/` at first.

### Format of aux_dictionary.tsv

| key      | value    | base_key | base_value | cost_offset |
| :------: | :------: | :------: | :--------: |:-----------:|
| あるぱか | アルパカ | かぴばら | カピバラ | -1          |

* aux_dictionary.tsv is a list of additional words to be added to Mozc's word dictionary.
* key and value (i.e. アルパカ) are fields for the new word.
* base_key and base_value (i.e. カピバラ) are fields of the reference word already in the dictionary.
* cost_offset is the difference from the base cost. (i.e. -1 makes the new word prioritized than the base word).
* other fields of the new word (e.g. lid, rid , cost) are copied from the reference word.

## dictionary_filter.tsv

The entries in this file are used to filter out the entries in the dictionary.

### Format of dictionary_filter.tsv

| key      | value    |
| :------: | :------: |
| あるぱか | 或るパカ |

* `key` and `value` are the regexp patterns of words to be removed.
* Each line is extended to the following regexp pattern:
  + `'^{key}\t\\d+\t\\d+\t\\d+\t{value}(\t.*)?\n$'`

## dictionary_ut_*.txt

Additional system-dictionary entries merged from the Mozc UT dictionaries.
Words already present in `dictionary0*.txt` are omitted. Every added entry uses
the `名詞,一般` POS id. Costs are the ones shipped by each UT dictionary, which
are high enough that an existing common word still wins a shared reading.

Sources, via https://github.com/utuhiro78/merge-ut-dictionaries :

*   [mozcdic-ut-jawiki](https://github.com/utuhiro78/mozcdic-ut-jawiki): CC BY-SA
*   [mozcdic-ut-neologd](https://github.com/utuhiro78/mozcdic-ut-neologd): see
    mecab-ipadic-NEologd `COPYING`
*   [mozcdic-ut-personal-names](https://github.com/utuhiro78/mozcdic-ut-personal-names):
    Apache License 2.0
*   [mozcdic-ut-place-names](https://github.com/utuhiro78/mozcdic-ut-place-names):
    Japan Post postal-code data, public domain
*   [mozcdic-ut-sudachidict](https://github.com/utuhiro78/mozcdic-ut-sudachidict):
    Apache License 2.0

Regenerate with `src/dictionary/import_ut_dictionary.py`, then split the output
into these files. They are inputs of `:base_dictionary_data`.
