// This test is adopted from V8's test suite.
// See deps/v8/test/mjsunit/instanceof.js in Node.js source repository.
//
// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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
'use strict';

const common = require('../../common');
const addon = require(`./build/${common.buildType}/test_general`);
const assert = require('assert');

assert.ok(addon.doInstanceOf({}, Object));
assert.ok(addon.doInstanceOf([], Object));

assert.ok(!addon.doInstanceOf({}, Array));
assert.ok(addon.doInstanceOf([], Array));

function TestChains() {
  const A = {};
  const B = {};
  const C = {};
  Object.setPrototypeOf(B, A);
  Object.setPrototypeOf(C, B);

  function F() { }
  F.prototype = A;
  assert.ok(addon.doInstanceOf(C, F));
  assert.ok(addon.doInstanceOf(B, F));
  assert.ok(!addon.doInstanceOf(A, F));

  F.prototype = B;
  assert.ok(addon.doInstanceOf(C, F));
  assert.ok(!addon.doInstanceOf(B, F));
  assert.ok(!addon.doInstanceOf(A, F));

  F.prototype = C;
  assert.ok(!addon.doInstanceOf(C, F));
  assert.ok(!addon.doInstanceOf(B, F));
  assert.ok(!addon.doInstanceOf(A, F));
}

TestChains();

function TestExceptions() {
  function F() { }
  const items = [ 1, new Number(42),
                  true,
                  'string', new String('hest'),
                  {}, [],
                  F, new F(),
                  Object, String ];

  let exceptions = 0;
  let instanceofs = 0;

  for (let i = 0; i < items.length; i++) {
    for (let j = 0; j < items.length; j++) {
      try {
        if (addon.doInstanceOf(items[i], items[j])) instanceofs++;
      } catch (e) {
        assert.ok(addon.doInstanceOf(e, TypeError));
        exceptions++;
      }
    }
  }
  assert.strictEqual(instanceofs, 10);
  assert.strictEqual(exceptions, 88);

  // Make sure to throw an exception if the function prototype
  // isn't a proper JavaScript object.
  function G() { }
  G.prototype = undefined;
  assert.throws(function() {
    addon.doInstanceOf({}, G);
  }, Error);
}

TestExceptions();
