// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.defineProperty(Number.prototype, "0",
    { set: function(v) { set = v; }});
Object.defineProperty(String.prototype, "0",
    { set: function(v) { set = v; }});
Object.defineProperty(String.prototype, "3",
    { set: function(v) { set = v; }});

var set;
var n = 1;
set = 0;
n[0] = 100;
assertEquals(100, set);
var s = "bla";
s[0] = 200;
assertEquals(100, set);
s[3] = 300;
assertEquals(300, set);

assertThrows(function(){"use strict"; var o = "123"; o[1] = 10; });
assertThrows(function(){"use strict"; var o = ""; o[1] = 10; });
assertThrows(function(){"use strict"; var o = 1; o[1] = 10; });

assertThrows(function() {
  "use strict";
  var sym = Symbol('66');
  sym.a = 0;
});

assertThrows(function() {
  "use strict";
  var sym = Symbol('66');
  sym['a' + 'b'] = 0;
});

assertThrows(function() {
  "use strict";
  var sym = Symbol('66');
  sym[62] = 0;
});

assertThrows(function() {
  "use strict";
  var o = "bla";
  o["0"] = 1;
});
