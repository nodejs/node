// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug;

Debug.setListener(function() {});

function main() {
  function* boo() {
    debugger;
    yield 42;
  }

  var gen = boo();
  gen.next();
  Debug.setBreakPoint(main, 0, 0);
  gen.next();
}

main();
