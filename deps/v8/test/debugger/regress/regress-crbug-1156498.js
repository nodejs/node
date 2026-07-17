// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const Debug = debug.Debug;
Debug.setListener(function (event, exec_state, event_data, data) {
});
Debug.setBreakOnException();

function f(){
  (() => {
    var out = 42;
    var needsOut = () => {return out}
    var simp_class = class {
      static var1  = unreferenced;
    }
  })();
}

assertThrows(f, ReferenceError);
