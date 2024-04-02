import { mustCall } from '../common/index.mjs';
import { Readable } from 'stream';
import assert from 'assert';

// These tests are manually ported from the draft PR for the test262 test suite
// Authored by Rick Waldron in https://github.com/tc39/test262/pull/2818/files

// test262 license:
// The << Software identified by reference to the Ecma Standard* ("Software)">>
// is protected by copyright and is being made available under the
// "BSD License", included below. This Software may be subject to third party
// rights (rights from parties other than Ecma International), including patent
// rights, and no licenses under such third party rights are granted under this
// license even if the third party concerned is a member of Ecma International.
// SEE THE ECMA CODE OF CONDUCT IN PATENT MATTERS AVAILABLE AT
// http://www.ecma-international.org/memento/codeofconduct.htm FOR INFORMATION
// REGARDING THE LICENSING OF PATENT CLAIMS THAT ARE REQUIRED TO IMPLEMENT ECMA
// INTERNATIONAL STANDARDS*

// Copyright (C) 2012-2013 Ecma International
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1.   Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
// 2.   Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
// 3.   Neither the name of the authors nor Ecma International may be used to
//      endorse or promote products derived from this software without specific
//      prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE ECMA INTERNATIONAL "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
// NO EVENT SHALL ECMA INTERNATIONAL BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
// OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// * Ecma International Standards hereafter means Ecma International Standards
// as well as Ecma Technical Reports


// Note all the tests that check AsyncIterator's prototype itself and things
// that happen before stream conversion were not ported.
{
  // asIndexedPairs/is-function
  assert.strictEqual(typeof Readable.prototype.asIndexedPairs, 'function');
  // asIndexedPairs/indexed-pairs.js
  const iterator = Readable.from([0, 1]);
  const indexedPairs = iterator.asIndexedPairs();

  for await (const [i, v] of indexedPairs) {
    assert.strictEqual(i, v);
  }
  // asIndexedPairs/length.js
  assert.strictEqual(Readable.prototype.asIndexedPairs.length, 0);
  const descriptor = Object.getOwnPropertyDescriptor(
    Readable.prototype,
    'asIndexedPairs'
  );
  assert.strictEqual(descriptor.enumerable, false);
  assert.strictEqual(descriptor.configurable, true);
  assert.strictEqual(descriptor.writable, true);
}
{
  // drop/length
  assert.strictEqual(Readable.prototype.drop.length, 1);
  const descriptor = Object.getOwnPropertyDescriptor(
    Readable.prototype,
    'drop'
  );
  assert.strictEqual(descriptor.enumerable, false);
  assert.strictEqual(descriptor.configurable, true);
  assert.strictEqual(descriptor.writable, true);
  // drop/limit-equals-total
  const iterator = Readable.from([1, 2]).drop(2);
  const result = await iterator[Symbol.asyncIterator]().next();
  assert.deepStrictEqual(result, { done: true, value: undefined });
  // drop/limit-greater-than-total.js
  const iterator2 = Readable.from([1, 2]).drop(3);
  const result2 = await iterator2[Symbol.asyncIterator]().next();
  assert.deepStrictEqual(result2, { done: true, value: undefined });
  // drop/limit-less-than-total.js
  const iterator3 = Readable.from([1, 2]).drop(1);
  const result3 = await iterator3[Symbol.asyncIterator]().next();
  assert.deepStrictEqual(result3, { done: false, value: 2 });
  // drop/limit-rangeerror
  assert.throws(() => Readable.from([1]).drop(-1), RangeError);
  assert.throws(() => {
    Readable.from([1]).drop({
      valueOf() {
        throw new Error('boom');
      }
    });
  }, /boom/);
  // drop/limit-tointeger
  const two = await Readable.from([1, 2]).drop({ valueOf: () => 1 }).toArray();
  assert.deepStrictEqual(two, [2]);
  // drop/name
  assert.strictEqual(Readable.prototype.drop.name, 'drop');
  // drop/non-constructible
  assert.throws(() => new Readable.prototype.drop(1), TypeError);
  // drop/proto
  const proto = Object.getPrototypeOf(Readable.prototype.drop);
  assert.strictEqual(proto, Function.prototype);
}
{
  // every/abrupt-iterator-close
  const stream = Readable.from([1, 2, 3]);
  const e = new Error();
  await assert.rejects(stream.every(mustCall(() => {
    throw e;
  }, 1)), e);
}
{
  // every/callable-fn
  await assert.rejects(Readable.from([1, 2]).every({}), TypeError);
}
{
  // every/callable
  Readable.prototype.every.call(Readable.from([]), () => {});
  // eslint-disable-next-line array-callback-return
  Readable.from([]).every(() => {});
  assert.throws(() => {
    const r = Readable.from([]);
    new r.every(() => {});
  }, TypeError);
}

{
  // every/false
  const iterator = Readable.from([1, 2, 3]);
  const result = await iterator.every((v) => v === 1);
  assert.strictEqual(result, false);
}
{
  // every/every
  const iterator = Readable.from([1, 2, 3]);
  const result = await iterator.every((v) => true);
  assert.strictEqual(result, true);
}

{
  // every/is-function
  assert.strictEqual(typeof Readable.prototype.every, 'function');
}
{
  // every/length
  assert.strictEqual(Readable.prototype.every.length, 1);
  // every/name
  assert.strictEqual(Readable.prototype.every.name, 'every');
  // every/propdesc
  const descriptor = Object.getOwnPropertyDescriptor(
    Readable.prototype,
    'every'
  );
  assert.strictEqual(descriptor.enumerable, false);
  assert.strictEqual(descriptor.configurable, true);
  assert.strictEqual(descriptor.writable, true);
}
