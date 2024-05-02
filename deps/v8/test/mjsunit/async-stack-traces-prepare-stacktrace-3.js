// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --async-stack-traces

// Check that Error.prepareStackTrace properly marks async frames.
Error.prepareStackTrace = (e, frames) => {
  assertSame(two, frames[0].getFunction());
  assertEquals(two.name, frames[0].getFunctionName());
  assertFalse(frames[0].isAsync());
  assertSame(one, frames[1].getFunction());
  assertEquals(one.name, frames[1].getFunctionName());
  assertTrue(frames[1].isAsync());
  return frames;
};

async function one(x) {
  return await two(x);
}

async function two(x) {
  try {
    x = await x;
    throw new Error();
  } catch (e) {
    return e.stack;
  }
}

one(1).catch(e => setTimeout(_ => {throw e}, 0));
