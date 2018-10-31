// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --async-stack-traces

// Check that Error.prepareStackTrace properly exposes async
// stack frames and special Promise.all() stack frames.
Error.prepareStackTrace = (e, frames) => {
  assertEquals(two, frames[0].getFunction());
  assertEquals(two.name, frames[0].getFunctionName());
  assertEquals(null, frames[0].getPromiseIndex());
  assertFalse(frames[0].isAsync());
  assertEquals(Promise.all, frames[1].getFunction());
  assertEquals(0, frames[1].getPromiseIndex());
  assertTrue(frames[1].isAsync());
  assertTrue(frames[1].isPromiseAll());
  assertEquals(one, frames[2].getFunction());
  assertEquals(one.name, frames[2].getFunctionName());
  assertEquals(null, frames[2].getPromiseIndex());
  assertTrue(frames[2].isAsync());
  assertFalse(frames[2].isPromiseAll());
  return frames;
};

async function one(x) {
  return await Promise.all([two(x)]);
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
