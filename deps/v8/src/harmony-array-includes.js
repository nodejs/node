// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Array = global.Array;

// -------------------------------------------------------------------

// Proposed for ES7
// https://github.com/tc39/Array.prototype.includes
// 6e3b78c927aeda20b9d40e81303f9d44596cd904
function ArrayIncludes(searchElement, fromIndex) {
  var array = ToObject(this);
  var len = ToLength(array.length);

  if (len === 0) {
    return false;
  }

  var n = ToInteger(fromIndex);

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
    if (SameValueZero(searchElement, elementK)) {
      return true;
    }

    ++k;
  }

  return false;
}

// -------------------------------------------------------------------

function HarmonyArrayIncludesExtendArrayPrototype() {
  %CheckIsBootstrapping();

  %FunctionSetLength(ArrayIncludes, 1);

  // Set up the non-enumerable functions on the Array prototype object.
  InstallFunctions($Array.prototype, DONT_ENUM, $Array(
    "includes", ArrayIncludes
  ));
}

HarmonyArrayIncludesExtendArrayPrototype();
