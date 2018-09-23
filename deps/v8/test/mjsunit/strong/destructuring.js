// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-destructuring
// Flags: --harmony-arrow-functions --strong-mode --allow-natives-syntax

(function() {
  function f({ x = function() { return []; } }) { "use strong"; return x(); }
  var a = f({ x: undefined });
  assertTrue(%IsStrong(a));

  // TODO(rossberg): Loading non-existent properties during destructuring should
  // not throw in strong mode.
  assertThrows(function() { f({}); }, TypeError);

  function weakf({ x = function() { return []; } }) { return x(); }
  a = weakf({});
  assertFalse(%IsStrong(a));

  function outerf() { return []; }
  function f2({ x = outerf }) { "use strong"; return x(); }
  a = f2({ x: undefined });
  assertFalse(%IsStrong(a));
})();
