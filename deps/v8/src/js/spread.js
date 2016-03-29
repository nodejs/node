// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

'use strict';

// -------------------------------------------------------------------
// Imports
var InternalArray = utils.InternalArray;
var MakeTypeError;

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------

function SpreadArguments() {
  var count = arguments.length;
  var args = new InternalArray();

  for (var i = 0; i < count; ++i) {
    var array = arguments[i];
    var length = array.length;
    for (var j = 0; j < length; ++j) {
      args.push(array[j]);
    }
  }

  return args;
}


function SpreadIterable(collection) {
  if (IS_NULL_OR_UNDEFINED(collection)) {
    throw MakeTypeError(kNotIterable, collection);
  }

  var args = new InternalArray();
  for (var value of collection) {
    args.push(value);
  }
  return args;
}

// ----------------------------------------------------------------------------
// Exports

%InstallToContext([
  "spread_arguments", SpreadArguments,
  "spread_iterable", SpreadIterable,
]);

})
