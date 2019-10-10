// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');

/*
 * This test makes sure that non-integer timer delays do not make the process
 * hang. See https://github.com/joyent/node/issues/8065 and
 * https://github.com/joyent/node/issues/8068 which have been fixed by
 * https://github.com/joyent/node/pull/8073.
 *
 * If the process hangs, this test will make the tests suite timeout,
 * otherwise it will exit very quickly (after 50 timers with a short delay
 * fire).
 *
 * We have to set at least several timers with a non-integer delay to
 * reproduce the issue. Sometimes, a timer with a non-integer delay will
 * expire correctly. 50 timers has always been more than enough to reproduce
 * it 100%.
 */

const TIMEOUT_DELAY = 1.1;
let N = 50;

const interval = setInterval(common.mustCall(() => {
  if (--N === 0) {
    clearInterval(interval);
  }
}, N), TIMEOUT_DELAY);

// Test non-integer delay ordering
{
  const ordering = [];

  setTimeout(common.mustCall(() => {
    ordering.push(1);
  }), 1);

  setTimeout(common.mustCall(() => {
    ordering.push(2);
  }), 1.8);

  setTimeout(common.mustCall(() => {
    ordering.push(3);
  }), 1.1);

  setTimeout(common.mustCall(() => {
    ordering.push(4);
  }), 1);

  setTimeout(common.mustCall(() => {
    const expected = [1, 2, 3, 4];

    assert.deepStrictEqual(
      ordering,
      expected,
      `Non-integer delay ordering should be ${expected}, but got ${ordering}`
    );

    // 2 should always be last of these delays due to ordering guarantees by
    // the implementation.
  }), 2);
}
