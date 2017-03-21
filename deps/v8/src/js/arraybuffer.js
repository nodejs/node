// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalArrayBuffer = global.ArrayBuffer;
var MaxSimple;
var MinSimple;
var SpeciesConstructor;

utils.Import(function(from) {
  MaxSimple = from.MaxSimple;
  MinSimple = from.MinSimple;
  SpeciesConstructor = from.SpeciesConstructor;
});

// -------------------------------------------------------------------

// ES6 Draft 15.13.5.5.3
function ArrayBufferSlice(start, end) {
  if (!IS_ARRAYBUFFER(this)) {
    throw %make_type_error(kIncompatibleMethodReceiver,
                        'ArrayBuffer.prototype.slice', this);
  }

  var relativeStart = TO_INTEGER(start);
  if (!IS_UNDEFINED(end)) {
    end = TO_INTEGER(end);
  }
  var first;
  var byte_length = %_ArrayBufferGetByteLength(this);
  if (relativeStart < 0) {
    first = MaxSimple(byte_length + relativeStart, 0);
  } else {
    first = MinSimple(relativeStart, byte_length);
  }
  var relativeEnd = IS_UNDEFINED(end) ? byte_length : end;
  var fin;
  if (relativeEnd < 0) {
    fin = MaxSimple(byte_length + relativeEnd, 0);
  } else {
    fin = MinSimple(relativeEnd, byte_length);
  }

  if (fin < first) {
    fin = first;
  }
  var newLen = fin - first;
  var constructor = SpeciesConstructor(this, GlobalArrayBuffer, true);
  var result = new constructor(newLen);
  if (!IS_ARRAYBUFFER(result)) {
    throw %make_type_error(kIncompatibleMethodReceiver,
                        'ArrayBuffer.prototype.slice', result);
  }
  // Checks for detached source/target ArrayBuffers are done inside of
  // %ArrayBufferSliceImpl; the reordering of checks does not violate
  // the spec because all exceptions thrown are TypeErrors.
  if (result === this) {
    throw %make_type_error(kArrayBufferSpeciesThis);
  }
  if (%_ArrayBufferGetByteLength(result) < newLen) {
    throw %make_type_error(kArrayBufferTooShort);
  }

  %ArrayBufferSliceImpl(this, result, first, newLen);
  return result;
}

utils.InstallFunctions(GlobalArrayBuffer.prototype, DONT_ENUM, [
  "slice", ArrayBufferSlice
]);

})
