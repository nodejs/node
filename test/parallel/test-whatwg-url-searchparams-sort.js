'use strict';

require('../common');
const { URL, URLSearchParams } = require('url');
const { test, assert_equals, assert_array_equals } = require('../common/wpt');

/* The following tests are copied from WPT. Modifications to them should be
   upstreamed first. Refs:
   https://github.com/w3c/web-platform-tests/blob/70a0898763/url/urlsearchparams-sort.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
/* eslint-disable */
[
  {
    "input": "z=b&a=b&z=a&a=a",
    "output": [["a", "b"], ["a", "a"], ["z", "b"], ["z", "a"]]
  },
  {
    "input": "\uFFFD=x&\uFFFC&\uFFFD=a",
    "output": [["\uFFFC", ""], ["\uFFFD", "x"], ["\uFFFD", "a"]]
  },
  {
    "input": "ﬃ&🌈", // 🌈 > code point, but < code unit because two code units
    "output": [["🌈", ""], ["ﬃ", ""]]
  },
  {
    "input": "é&e\uFFFD&e\u0301",
    "output": [["e\u0301", ""], ["e\uFFFD", ""], ["é", ""]]
  },
  {
    "input": "z=z&a=a&z=y&a=b&z=x&a=c&z=w&a=d&z=v&a=e&z=u&a=f&z=t&a=g",
    "output": [["a", "a"], ["a", "b"], ["a", "c"], ["a", "d"], ["a", "e"], ["a", "f"], ["a", "g"], ["z", "z"], ["z", "y"], ["z", "x"], ["z", "w"], ["z", "v"], ["z", "u"], ["z", "t"]]
  }
].forEach((val) => {
  test(() => {
    let params = new URLSearchParams(val.input),
        i = 0
    params.sort()
    for(let param of params) {
      assert_array_equals(param, val.output[i])
      i++
    }
  }, `Parse and sort: ${val.input}`)

  test(() => {
    let url = new URL(`?${val.input}`, "https://example/")
    url.searchParams.sort()
    let params = new URLSearchParams(url.search),
        i = 0
    for(let param of params) {
      assert_array_equals(param, val.output[i])
      i++
    }
  }, `URL parse and sort: ${val.input}`)
})

test(function() {
  const url = new URL("http://example.com/?")
  url.searchParams.sort()
  assert_equals(url.href, "http://example.com/")
  assert_equals(url.search, "")
}, "Sorting non-existent params removes ? from URL")
/* eslint-enable */

// Tests below are not from WPT.

// Test bottom-up iterative stable merge sort
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
