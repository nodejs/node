// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --cache=code --no-debug-code

try { } catch(e) { }
try { try { } catch (e) { } } catch(e) { }
try {
  var Debug = %GetDebugContext().Debug;
  Debug.setListener(function(){});
} catch(e) { }
(function() {
  Debug.setBreakPoint(function(){}, 0, 0);
})();

var a = 1;
a += a;
Debug.setListener(null);
assertEquals(2, a);
