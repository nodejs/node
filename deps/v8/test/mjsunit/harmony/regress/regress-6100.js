// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test is added because harmony-template-escapes were not properly
// handled in the preparser.

function check({cooked, raw, exprs}) {
  return function(strs, ...args) {
    assertArrayEquals(cooked, strs);
    assertArrayEquals(raw, strs.raw);
    assertArrayEquals(exprs, args);
  };
}

// clang-format off

function lazy() {
  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\01'
    ],
    'exprs': []
  })`\01`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\01',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\01${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\01'
    ],
    'exprs': [
      0
    ]
  })`left${0}\01`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\01',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\01${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\1'
    ],
    'exprs': []
  })`\1`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\1',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\1${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\1'
    ],
    'exprs': [
      0
    ]
  })`left${0}\1`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\1',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\1${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\xg'
    ],
    'exprs': []
  })`\xg`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\xg',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\xg${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\xg'
    ],
    'exprs': [
      0
    ]
  })`left${0}\xg`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\xg',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\xg${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\xAg'
    ],
    'exprs': []
  })`\xAg`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\xAg',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\xAg${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\xAg'
    ],
    'exprs': [
      0
    ]
  })`left${0}\xAg`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\xAg',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\xAg${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u0'
    ],
    'exprs': []
  })`\u0`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u0',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u0${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u0'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u0`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u0',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u0${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u0g'
    ],
    'exprs': []
  })`\u0g`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u0g',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u0g${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u0g'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u0g`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u0g',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u0g${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u00g'
    ],
    'exprs': []
  })`\u00g`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u00g',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u00g${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u00g'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u00g`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u00g',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u00g${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u000g'
    ],
    'exprs': []
  })`\u000g`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u000g',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u000g${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u000g'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u000g`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u000g',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u000g${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{}'
    ],
    'exprs': []
  })`\u{}`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{}',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{}${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{}'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{}`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{}',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{}${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{-0}'
    ],
    'exprs': []
  })`\u{-0}`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{-0}',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{-0}${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{-0}'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{-0}`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{-0}',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{-0}${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{g}'
    ],
    'exprs': []
  })`\u{g}`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{g}',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{g}${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{g}'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{g}`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{g}',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{g}${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{0'
    ],
    'exprs': []
  })`\u{0`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{0',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{0${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{0'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{0`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{0',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{0${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{\\u{0}'
    ],
    'exprs': []
  })`\u{\u{0}`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{\\u{0}',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{\u{0}${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{\\u{0}'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{\u{0}`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{\\u{0}',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{\u{0}${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{110000}'
    ],
    'exprs': []
  })`\u{110000}`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{110000}',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{110000}${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{110000}'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{110000}`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{110000}',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{110000}${1}right`;



  function checkMultiple(expectedArray) {
    let results = [];
    return function consume(strs, ...args) {
      if (typeof strs === 'undefined') {
        assertArrayEquals(expectedArray, results);
      } else {
        results.push({cooked: strs, raw: strs.raw, exprs: args});
        return consume;
      }
    };
  }


  checkMultiple([{
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u',
    ],
    'exprs': []
  }, {
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u',
    ],
    'exprs': []
  }])`\u``\u`();

  checkMultiple([{
    'cooked': [
      ' '
    ],
    'raw': [
      ' ',
    ],
    'exprs': []
  }, {
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u',
    ],
    'exprs': []
  }])` ``\u`();

  checkMultiple([{
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u',
    ],
    'exprs': []
  }, {
    'cooked': [
      ' '
    ],
    'raw': [
      ' ',
    ],
    'exprs': []
  }])`\u`` `();

  checkMultiple([{
    'cooked': [
      ' '
    ],
    'raw': [
      ' ',
    ],
    'exprs': []
  }, {
    'cooked': [
      ' '
    ],
    'raw': [
      ' ',
    ],
    'exprs': []
  }])` `` `();
}

(function eager() {
  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\01'
    ],
    'exprs': []
  })`\01`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\01',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\01${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\01'
    ],
    'exprs': [
      0
    ]
  })`left${0}\01`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\01',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\01${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\1'
    ],
    'exprs': []
  })`\1`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\1',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\1${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\1'
    ],
    'exprs': [
      0
    ]
  })`left${0}\1`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\1',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\1${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\xg'
    ],
    'exprs': []
  })`\xg`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\xg',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\xg${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\xg'
    ],
    'exprs': [
      0
    ]
  })`left${0}\xg`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\xg',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\xg${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\xAg'
    ],
    'exprs': []
  })`\xAg`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\xAg',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\xAg${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\xAg'
    ],
    'exprs': [
      0
    ]
  })`left${0}\xAg`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\xAg',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\xAg${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u0'
    ],
    'exprs': []
  })`\u0`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u0',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u0${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u0'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u0`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u0',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u0${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u0g'
    ],
    'exprs': []
  })`\u0g`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u0g',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u0g${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u0g'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u0g`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u0g',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u0g${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u00g'
    ],
    'exprs': []
  })`\u00g`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u00g',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u00g${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u00g'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u00g`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u00g',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u00g${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u000g'
    ],
    'exprs': []
  })`\u000g`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u000g',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u000g${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u000g'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u000g`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u000g',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u000g${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{}'
    ],
    'exprs': []
  })`\u{}`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{}',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{}${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{}'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{}`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{}',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{}${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{-0}'
    ],
    'exprs': []
  })`\u{-0}`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{-0}',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{-0}${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{-0}'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{-0}`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{-0}',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{-0}${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{g}'
    ],
    'exprs': []
  })`\u{g}`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{g}',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{g}${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{g}'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{g}`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{g}',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{g}${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{0'
    ],
    'exprs': []
  })`\u{0`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{0',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{0${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{0'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{0`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{0',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{0${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{\\u{0}'
    ],
    'exprs': []
  })`\u{\u{0}`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{\\u{0}',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{\u{0}${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{\\u{0}'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{\u{0}`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{\\u{0}',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{\u{0}${1}right`;

  check({
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u{110000}'
    ],
    'exprs': []
  })`\u{110000}`;

  check({
    'cooked': [
      undefined,
      'right'
    ],
    'raw': [
      '\\u{110000}',
      'right'
    ],
    'exprs': [
      0
    ]
  })`\u{110000}${0}right`;

  check({
    'cooked': [
      'left',
      undefined
    ],
    'raw': [
      'left',
      '\\u{110000}'
    ],
    'exprs': [
      0
    ]
  })`left${0}\u{110000}`;

  check({
    'cooked': [
      'left',
      undefined,
      'right'
    ],
    'raw': [
      'left',
      '\\u{110000}',
      'right'
    ],
    'exprs': [
      0,
      1
    ]
  })`left${0}\u{110000}${1}right`;



  function checkMultiple(expectedArray) {
    let results = [];
    return function consume(strs, ...args) {
      if (typeof strs === 'undefined') {
        assertArrayEquals(expectedArray, results);
      } else {
        results.push({cooked: strs, raw: strs.raw, exprs: args});
        return consume;
      }
    };
  }


  checkMultiple([{
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u',
    ],
    'exprs': []
  }, {
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u',
    ],
    'exprs': []
  }])`\u``\u`();

  checkMultiple([{
    'cooked': [
      ' '
    ],
    'raw': [
      ' ',
    ],
    'exprs': []
  }, {
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u',
    ],
    'exprs': []
  }])` ``\u`();

  checkMultiple([{
    'cooked': [
      undefined
    ],
    'raw': [
      '\\u',
    ],
    'exprs': []
  }, {
    'cooked': [
      ' '
    ],
    'raw': [
      ' ',
    ],
    'exprs': []
  }])`\u`` `();

  checkMultiple([{
    'cooked': [
      ' '
    ],
    'raw': [
      ' ',
    ],
    'exprs': []
  }, {
    'cooked': [
      ' '
    ],
    'raw': [
      ' ',
    ],
    'exprs': []
  }])` `` `();
})();

lazy();
