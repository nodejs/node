'use strict';

// Tests below are not from WPT.

require('../common');
const { URL, URLSearchParams } = require('url');
const { test, assert_array_equals } = require('../common/wpt').harness;

// TODO(joyeecheung): upstream this to WPT, if possible - even
// just as a test for large inputs. Other implementations may
// have a similar cutoff anyway.

// Test bottom-up iterative stable merge sort because we only use that
// algorithm to sort > 100 search params.
const tests = [{ input: '', output: [] }];
const pairs = [];
for (let i = 10; i < 100; i++) {
  pairs.push([`a${i}`, 'b']);
  tests[0].output.push([`a${i}`, 'b']);
}
tests[0].input = pairs.sort(() => Math.random() > 0.5)
  .map((pair) => pair.join('=')).join('&');

tests.push(
  {
    'input': 'z=a&=b&c=d',
    'output': [['', 'b'], ['c', 'd'], ['z', 'a']]
  }
);

tests.forEach((val) => {
  test(() => {
    const params = new URLSearchParams(val.input);
    let i = 0;
    params.sort();
    for (const param of params) {
      assert_array_equals(param, val.output[i]);
      i++;
    }
  }, `Parse and sort: ${val.input}`);

  test(() => {
    const url = new URL(`?${val.input}`, 'https://example/');
    url.searchParams.sort();
    const params = new URLSearchParams(url.search);
    let i = 0;
    for (const param of params) {
      assert_array_equals(param, val.output[i]);
      i++;
    }
  }, `URL parse and sort: ${val.input}`);
});
