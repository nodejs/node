// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --async-stack-traces

// Check that Error.prepareStackTrace doesn't expose strict
// mode closures, even in the presence of async frames.
Error.prepareStackTrace = (e, frames) => {
  assertEquals(two, frames[0].getFunction());
  assertEquals(two.name, frames[0].getFunctionName());
  assertEquals(undefined, frames[1].getFunction());
  assertEquals(one.name, frames[1].getFunctionName());
  return frames;
};

async function one(x) {
  "use strict";
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
