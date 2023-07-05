// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

let thisValue;
function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    thisValue = exec_state.frame(0).evaluate('this').value();
  }
};

Debug.setListener(listener);

class Foo {}
class Bar extends Foo {
  constructor() {
    super();
    var b = () => this;
    this.c = 'b';  // <-- Inspect 'this' (it will be undefined)
    debugger;
  }
}

new Bar();

Debug.setListener(null);

assertNotEquals(undefined, thisValue);
