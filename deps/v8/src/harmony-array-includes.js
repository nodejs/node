// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

'use strict';

%CheckIsBootstrapping();

var GlobalArray = global.Array;

// -------------------------------------------------------------------

// Proposed for ES7
// https://github.com/tc39/Array.prototype.includes
// 6e3b78c927aeda20b9d40e81303f9d44596cd904
function ArrayIncludes(searchElement, fromIndex) {
  var array = $toObject(this);
  var len = $toLength(array.length);

  if (len === 0) {
    return false;
  }

  var n = $toInteger(fromIndex);

  var k;
  if (n >= 0) {
    k = n;
  } else {
    k = len + n;
    if (k < 0) {
      k = 0;
    }
  }

  while (k < len) {
    var elementK = array[k];
    if ($sameValueZero(searchElement, elementK)) {
      return true;
    }

    ++k;
  }

  return false;
}

// -------------------------------------------------------------------

%FunctionSetLength(ArrayIncludes, 1);

// Set up the non-enumerable functions on the Array prototype object.
utils.InstallFunctions(GlobalArray.prototype, DONT_ENUM, [
  "includes", ArrayIncludes
]);

})
