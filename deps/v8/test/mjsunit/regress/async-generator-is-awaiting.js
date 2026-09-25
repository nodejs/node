// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Async generator builtins that suspend the generator set the is_awaiting bit
// to 1 before awaiting. This is cleared when resumed. This tests that the bit
// is set after the await operation successfully completes (i.e. returns the
// Promise), since it can throw, and that thrown exception can be caught by
// script. Otherwise the is_awaiting bit won't be cleared.

// This makes `await new Promise(() => {})` throw.
Object.defineProperty(Promise.prototype, 'constructor', {
  get() { throw 42; }
});

// AsyncGeneratorAwait
{
  async function *f() {
    try {
      await new Promise(() => {});
    } catch (e) {
    }
  }

  f().next();
}

// AsyncGeneratorYield
{
  async function *f() {
    try {
      yield new Promise(() => {});
    } catch (e) {
    }
  }

  f().next();
}

// AsyncGeneratorReturn isn't affected because it's not possible, in script, to
// catch an error thrown by a return resumption. It'll be caught by the
// synthetic try-catch around the whole body of the async generator, which will
// correctly reset the is_awaiting bit.
