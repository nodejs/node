// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalObject = global.Object;

var OwnPropertyKeys;

utils.Import(function(from) {
  OwnPropertyKeys = from.OwnPropertyKeys;
});

// -------------------------------------------------------------------

// ES6, draft 04-03-15, section 19.1.2.1
function ObjectAssign(target, sources) {
  var to = TO_OBJECT_INLINE(target);
  var argsLen = %_ArgumentsLength();
  if (argsLen < 2) return to;

  for (var i = 1; i < argsLen; ++i) {
    var nextSource = %_Arguments(i);
    if (IS_NULL_OR_UNDEFINED(nextSource)) {
      continue;
    }

    var from = TO_OBJECT_INLINE(nextSource);
    var keys = OwnPropertyKeys(from);
    var len = keys.length;

    for (var j = 0; j < len; ++j) {
      var key = keys[j];
      if (%IsPropertyEnumerable(from, key)) {
        var propValue = from[key];
        to[key] = propValue;
      }
    }
  }
  return to;
}

// Set up non-enumerable functions on the Object object.
utils.InstallFunctions(GlobalObject, DONT_ENUM, [
  "assign", ObjectAssign
]);

})
