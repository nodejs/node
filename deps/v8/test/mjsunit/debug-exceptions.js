// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug


Debug = debug.Debug

let error = false;
let uncaught;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Exception) return;
  try {
    uncaught = event_data.uncaught();
  } catch (e) {
    error = true;
  }
}

Debug.setBreakOnException();
Debug.setListener(listener);


function assertCaught(f) {
  try {f()} finally {
    assertFalse(uncaught);
    return;
  }
}

function assertUncaught(f) {
  try {f()} finally {
    assertTrue(uncaught);
    return;
  }
}


assertUncaught(() => {
  for (var a of [1, 2, 3]) {
    throw a
  }
});

assertUncaught(() => {
  for (var a of [1, 2, 3]) {
    try {throw a} finally {}
  }
});

assertCaught(() => {
  for (var a of [1, 2, 3]) {
    try {
      try {throw a} finally {}
    } catch(_) {}
  }
});

assertCaught(() => {
  try {
    for (var a of [1, 2, 3]) {
      try {throw a} finally {}
    }
  } catch(_) {}
});


// Check that an internal exception in our yield* desugaring is not observable.
{
  uncaught = null;

  let iter = {
    next() {return {value:42, done:false}},
    throw() {return {done:true}}
  };
  let iterable = {[Symbol.iterator]() {return iter}};
  function* f() { yield* iterable }

  let g = f();
  g.next();
  assertEquals({value: undefined, done: true}, g.throw());
  assertNull(uncaught);  // No exception event was generated.
}


assertFalse(error);
