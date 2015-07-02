// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

var GlobalSharedArrayBuffer = global.SharedArrayBuffer;
var GlobalObject = global.Object;

// -------------------------------------------------------------------

function SharedArrayBufferConstructor(length) { // length = 1
  if (%_IsConstructCall()) {
    var byteLength = $toPositiveInteger(length, kInvalidArrayBufferLength);
    %ArrayBufferInitialize(this, byteLength, kShared);
  } else {
    throw MakeTypeError(kConstructorNotFunction, "SharedArrayBuffer");
  }
}

function SharedArrayBufferGetByteLen() {
  if (!IS_SHAREDARRAYBUFFER(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'SharedArrayBuffer.prototype.byteLength', this);
  }
  return %_ArrayBufferGetByteLength(this);
}

function SharedArrayBufferIsViewJS(obj) {
  return %ArrayBufferIsView(obj);
}


// Set up the SharedArrayBuffer constructor function.
%SetCode(GlobalSharedArrayBuffer, SharedArrayBufferConstructor);
%FunctionSetPrototype(GlobalSharedArrayBuffer, new GlobalObject());

// Set up the constructor property on the SharedArrayBuffer prototype object.
%AddNamedProperty(GlobalSharedArrayBuffer.prototype, "constructor",
                  GlobalSharedArrayBuffer, DONT_ENUM);

%AddNamedProperty(GlobalSharedArrayBuffer.prototype,
    symbolToStringTag, "SharedArrayBuffer", DONT_ENUM | READ_ONLY);

utils.InstallGetter(GlobalSharedArrayBuffer.prototype, "byteLength",
                    SharedArrayBufferGetByteLen);

utils.InstallFunctions(GlobalSharedArrayBuffer, DONT_ENUM, [
    "isView", SharedArrayBufferIsViewJS
]);

})
