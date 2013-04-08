// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"use strict";

var $ArrayBuffer = global.__ArrayBuffer;

function ArrayBufferConstructor(byteLength) { // length = 1
  if (%_IsConstructCall()) {
    var l = TO_POSITIVE_INTEGER(byteLength);
    %ArrayBufferInitialize(this, l);
  } else {
    return new $ArrayBuffer(byteLength);
  }
}

function ArrayBufferGetByteLength() {
  if (!IS_ARRAYBUFFER(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['ArrayBuffer.prototype.byteLength', this]);
  }
  return %ArrayBufferGetByteLength(this);
}

// ES6 Draft 15.13.5.5.3
function ArrayBufferSlice(start, end) {
  if (!IS_ARRAYBUFFER(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['ArrayBuffer.prototype.slice', this]);
  }

  var relativeStart = TO_INTEGER(start);
  var first;
  if (relativeStart < 0) {
    first = MathMax(this.byteLength + relativeStart, 0);
  } else {
    first = MathMin(relativeStart, this.byteLength);
  }
  var relativeEnd = IS_UNDEFINED(end) ? this.byteLength : TO_INTEGER(end);
  var fin;
  if (relativeEnd < 0) {
    fin = MathMax(this.byteLength + relativeEnd, 0);
  } else {
    fin = MathMin(relativeEnd, this.byteLength);
  }

  var newLen = fin - first;
  // TODO(dslomov): implement inheritance
  var result = new $ArrayBuffer(newLen);

  %ArrayBufferSliceImpl(this, result, first);
  return result;
}



// -------------------------------------------------------------------

(function () {
  %CheckIsBootstrapping();

  // Set up the Uint16Array constructor function.
  %SetCode($ArrayBuffer, ArrayBufferConstructor);

  // Set up the constructor property on the ArrayBuffer prototype object.
  %SetProperty($ArrayBuffer.prototype, "constructor", $ArrayBuffer, DONT_ENUM);

  InstallGetter($ArrayBuffer.prototype, "byteLength", ArrayBufferGetByteLength);

  InstallFunctions($ArrayBuffer.prototype, DONT_ENUM, $Array(
      "slice", ArrayBufferSlice
  ));

})();
