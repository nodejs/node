// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


'use strict';
if ((typeof mojo !== 'undefined')  && mojo.internal && mojo.config) {

(function() {
  var mojomId = 'mojo/public/mojom/base/string16.mojom';
  if (mojo.internal.isMojomLoaded(mojomId)) {
    console.warn('The following mojom is loaded multiple times: ' + mojomId);
    return;
  }
  mojo.internal.markMojomLoaded(mojomId);

  // TODO(yzshen): Define these aliases to minimize the differences between the
  // old/new modes. Remove them when the old mode goes away.
  var bindings = mojo;
  var associatedBindings = mojo;
  var codec = mojo.internal;
  var validator = mojo.internal;

  var exports = mojo.internal.exposeNamespace('mojo.common.mojom');



  function String16(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  String16.prototype.initDefaults_ = function() {
    this.data = null;
  };
  String16.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  String16.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 16}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;


    
    // validate String16.data
    err = messageValidator.validateArrayPointer(offset + codec.kStructHeaderSize + 0, 2, codec.Uint16, false, [0], 0);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  String16.encodedSize = codec.kStructHeaderSize + 8;

  String16.decode = function(decoder) {
    var packed;
    var val = new String16();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.data = decoder.decodeArrayPointer(codec.Uint16);
    return val;
  };

  String16.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(String16.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeArrayPointer(codec.Uint16, val.data);
  };
  exports.String16 = String16;
})();
}

if ((typeof mojo === 'undefined') || !mojo.internal || !mojo.config) {

define("mojo/public/mojom/base/string16.mojom", [
    "mojo/public/js/associated_bindings",
    "mojo/public/js/bindings",
    "mojo/public/js/codec",
    "mojo/public/js/core",
    "mojo/public/js/validator",
], function(associatedBindings, bindings, codec, core, validator) {
  var exports = {};

  function String16(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  String16.prototype.initDefaults_ = function() {
    this.data = null;
  };
  String16.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  String16.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 16}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;


    
    // validate String16.data
    err = messageValidator.validateArrayPointer(offset + codec.kStructHeaderSize + 0, 2, codec.Uint16, false, [0], 0);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  String16.encodedSize = codec.kStructHeaderSize + 8;

  String16.decode = function(decoder) {
    var packed;
    var val = new String16();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.data = decoder.decodeArrayPointer(codec.Uint16);
    return val;
  };

  String16.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(String16.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeArrayPointer(codec.Uint16, val.data);
  };
  exports.String16 = String16;

  return exports;
});
}