// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-destructuring-bind
// Flags: --strong-mode --allow-natives-syntax

(function() {
  var f = (function() {
    "use strong";
    return function f({ x = function() { return []; } }) { return x(); };
  })();
  var a = f({ x: undefined });
  assertTrue(%IsStrong(a));

  // TODO(rossberg): Loading non-existent properties during destructuring should
  // not throw in strong mode.
  assertThrows(function() { f({}); }, TypeError);

  function weakf({ x = function() { return []; } }) { return x(); }
  a = weakf({});
  assertFalse(%IsStrong(a));

  function outerf() { return []; }
  var f2 = (function() {
    "use strong";
    return function f2({ x = outerf }) { return x(); };
  })();
  a = f2({ x: undefined });
  assertFalse(%IsStrong(a));
})();
