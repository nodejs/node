// META: title=Blob/Unicode interaction: normalization and encoding

'use strict';

const OMICRON_WITH_OXIA = '\u1F79'; // NFC normalized to U+3CC
const CONTAINS_UNPAIRED_SURROGATES = 'abc\uDC00def\uD800ghi';
const REPLACED = 'abc\uFFFDdef\uFFFDghi';

promise_test(async t => {
  const blob = new Blob([OMICRON_WITH_OXIA]);
  const result = await blob.text();
  assert_equals(result, OMICRON_WITH_OXIA, 'String should not be normalized');
}, 'Test that strings are not NFC normalized by Blob constructor');

promise_test(async t => {
  const file = new File([OMICRON_WITH_OXIA], 'name');
  const result = await file.text();
  assert_equals(result, OMICRON_WITH_OXIA, 'String should not be normalized');
}, 'Test that strings are not NFC normalized by File constructor');

promise_test(async t => {
  const blob = new Blob([CONTAINS_UNPAIRED_SURROGATES]);
  const result = await blob.text();
  assert_equals(result, REPLACED, 'Unpaired surrogates should be replaced.');
}, 'Test that unpaired surrogates are replaced by Blob constructor');

promise_test(async t => {
  const file = new File([CONTAINS_UNPAIRED_SURROGATES], 'name');
  const result = await file.text();
  assert_equals(result, REPLACED, 'Unpaired surrogates should be replaced.');
}, 'Test that unpaired surrogates are replaced by File constructor');
