// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

var failure = null;
var args;

function listener(event, exec_state, event_data, data) {
  if (event != debug.Debug.DebugEvent.Break) return;
  try {
    args = exec_state.frame(0).evaluate('arguments').value();
  } catch (e) {
    failure = e;
  }
}

debug.Debug.setListener(listener);

function* gen(a, b) {
  debugger;
  yield a;
  yield b;
}

var foo = gen(1, 2);

foo.next()
assertEquals(2, args.length);
assertEquals(undefined, args[0]);
assertEquals(undefined, args[1]);

foo.next()
assertEquals(2, args.length);
assertEquals(undefined, args[0]);
assertEquals(undefined, args[1]);

foo.next()
assertEquals(2, args.length);
assertEquals(undefined, args[0]);
assertEquals(undefined, args[1]);

assertNull(failure);
