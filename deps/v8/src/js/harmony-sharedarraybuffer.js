// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

var GlobalSharedArrayBuffer = global.SharedArrayBuffer;
var MakeTypeError;

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
})

// -------------------------------------------------------------------

function SharedArrayBufferGetByteLen() {
  if (!IS_SHAREDARRAYBUFFER(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'SharedArrayBuffer.prototype.byteLength', this);
  }
  return %_ArrayBufferGetByteLength(this);
}

utils.InstallGetter(GlobalSharedArrayBuffer.prototype, "byteLength",
                    SharedArrayBufferGetByteLen);

})
