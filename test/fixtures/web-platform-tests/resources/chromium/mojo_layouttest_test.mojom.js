// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {
  var mojomId = 'content/test/data/mojo_layouttest_test.mojom';
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

  var exports = mojo.internal.exposeNamespace('content.mojom');



  function MojoLayoutTestHelper_Reverse_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  MojoLayoutTestHelper_Reverse_Params.prototype.initDefaults_ = function() {
    this.message = null;
  };
  MojoLayoutTestHelper_Reverse_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  MojoLayoutTestHelper_Reverse_Params.validate = function(messageValidator, offset) {
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


    // validate MojoLayoutTestHelper_Reverse_Params.message
    err = messageValidator.validateStringPointer(offset + codec.kStructHeaderSize + 0, false)
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  MojoLayoutTestHelper_Reverse_Params.encodedSize = codec.kStructHeaderSize + 8;

  MojoLayoutTestHelper_Reverse_Params.decode = function(decoder) {
    var packed;
    var val = new MojoLayoutTestHelper_Reverse_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.message = decoder.decodeStruct(codec.String);
    return val;
  };

  MojoLayoutTestHelper_Reverse_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(MojoLayoutTestHelper_Reverse_Params.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.String, val.message);
  };
  function MojoLayoutTestHelper_Reverse_ResponseParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  MojoLayoutTestHelper_Reverse_ResponseParams.prototype.initDefaults_ = function() {
    this.reversed = null;
  };
  MojoLayoutTestHelper_Reverse_ResponseParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  MojoLayoutTestHelper_Reverse_ResponseParams.validate = function(messageValidator, offset) {
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


    // validate MojoLayoutTestHelper_Reverse_ResponseParams.reversed
    err = messageValidator.validateStringPointer(offset + codec.kStructHeaderSize + 0, false)
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  MojoLayoutTestHelper_Reverse_ResponseParams.encodedSize = codec.kStructHeaderSize + 8;

  MojoLayoutTestHelper_Reverse_ResponseParams.decode = function(decoder) {
    var packed;
    var val = new MojoLayoutTestHelper_Reverse_ResponseParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.reversed = decoder.decodeStruct(codec.String);
    return val;
  };

  MojoLayoutTestHelper_Reverse_ResponseParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(MojoLayoutTestHelper_Reverse_ResponseParams.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.String, val.reversed);
  };
  var kMojoLayoutTestHelper_Reverse_Name = 0;

  function MojoLayoutTestHelperPtr(handleOrPtrInfo) {
    this.ptr = new bindings.InterfacePtrController(MojoLayoutTestHelper,
                                                   handleOrPtrInfo);
  }

  function MojoLayoutTestHelperAssociatedPtr(associatedInterfacePtrInfo) {
    this.ptr = new associatedBindings.AssociatedInterfacePtrController(
        MojoLayoutTestHelper, associatedInterfacePtrInfo);
  }

  MojoLayoutTestHelperAssociatedPtr.prototype =
      Object.create(MojoLayoutTestHelperPtr.prototype);
  MojoLayoutTestHelperAssociatedPtr.prototype.constructor =
      MojoLayoutTestHelperAssociatedPtr;

  function MojoLayoutTestHelperProxy(receiver) {
    this.receiver_ = receiver;
  }
  MojoLayoutTestHelperPtr.prototype.reverse = function() {
    return MojoLayoutTestHelperProxy.prototype.reverse
        .apply(this.ptr.getProxy(), arguments);
  };

  MojoLayoutTestHelperProxy.prototype.reverse = function(message) {
    var params = new MojoLayoutTestHelper_Reverse_Params();
    params.message = message;
    return new Promise(function(resolve, reject) {
      var builder = new codec.MessageV1Builder(
          kMojoLayoutTestHelper_Reverse_Name,
          codec.align(MojoLayoutTestHelper_Reverse_Params.encodedSize),
          codec.kMessageExpectsResponse, 0);
      builder.encodeStruct(MojoLayoutTestHelper_Reverse_Params, params);
      var message = builder.finish();
      this.receiver_.acceptAndExpectResponse(message).then(function(message) {
        var reader = new codec.MessageReader(message);
        var responseParams =
            reader.decodeStruct(MojoLayoutTestHelper_Reverse_ResponseParams);
        resolve(responseParams);
      }).catch(function(result) {
        reject(Error("Connection error: " + result));
      });
    }.bind(this));
  };

  function MojoLayoutTestHelperStub(delegate) {
    this.delegate_ = delegate;
  }
  MojoLayoutTestHelperStub.prototype.reverse = function(message) {
    return this.delegate_ && this.delegate_.reverse && this.delegate_.reverse(message);
  }

  MojoLayoutTestHelperStub.prototype.accept = function(message) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    default:
      return false;
    }
  };

  MojoLayoutTestHelperStub.prototype.acceptWithResponder =
      function(message, responder) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    case kMojoLayoutTestHelper_Reverse_Name:
      var params = reader.decodeStruct(MojoLayoutTestHelper_Reverse_Params);
      this.reverse(params.message).then(function(response) {
        var responseParams =
            new MojoLayoutTestHelper_Reverse_ResponseParams();
        responseParams.reversed = response.reversed;
        var builder = new codec.MessageV1Builder(
            kMojoLayoutTestHelper_Reverse_Name,
            codec.align(MojoLayoutTestHelper_Reverse_ResponseParams.encodedSize),
            codec.kMessageIsResponse, reader.requestID);
        builder.encodeStruct(MojoLayoutTestHelper_Reverse_ResponseParams,
                             responseParams);
        var message = builder.finish();
        responder.accept(message);
      });
      return true;
    default:
      return false;
    }
  };

  function validateMojoLayoutTestHelperRequest(messageValidator) {
    var message = messageValidator.message;
    var paramsClass = null;
    switch (message.getName()) {
      case kMojoLayoutTestHelper_Reverse_Name:
        if (message.expectsResponse())
          paramsClass = MojoLayoutTestHelper_Reverse_Params;
      break;
    }
    if (paramsClass === null)
      return validator.validationError.NONE;
    return paramsClass.validate(messageValidator, messageValidator.message.getHeaderNumBytes());
  }

  function validateMojoLayoutTestHelperResponse(messageValidator) {
   var message = messageValidator.message;
   var paramsClass = null;
   switch (message.getName()) {
      case kMojoLayoutTestHelper_Reverse_Name:
        if (message.isResponse())
          paramsClass = MojoLayoutTestHelper_Reverse_ResponseParams;
        break;
    }
    if (paramsClass === null)
      return validator.validationError.NONE;
    return paramsClass.validate(messageValidator, messageValidator.message.getHeaderNumBytes());
  }

  var MojoLayoutTestHelper = {
    name: 'content.mojom.MojoLayoutTestHelper',
    kVersion: 0,
    ptrClass: MojoLayoutTestHelperPtr,
    proxyClass: MojoLayoutTestHelperProxy,
    stubClass: MojoLayoutTestHelperStub,
    validateRequest: validateMojoLayoutTestHelperRequest,
    validateResponse: validateMojoLayoutTestHelperResponse,
  };
  MojoLayoutTestHelperStub.prototype.validator = validateMojoLayoutTestHelperRequest;
  MojoLayoutTestHelperProxy.prototype.validator = validateMojoLayoutTestHelperResponse;
  exports.MojoLayoutTestHelper = MojoLayoutTestHelper;
  exports.MojoLayoutTestHelperPtr = MojoLayoutTestHelperPtr;
  exports.MojoLayoutTestHelperAssociatedPtr = MojoLayoutTestHelperAssociatedPtr;
})();
