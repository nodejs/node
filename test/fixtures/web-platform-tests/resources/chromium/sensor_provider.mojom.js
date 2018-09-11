// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {
  var mojomId = 'services/device/public/mojom/sensor_provider.mojom';
  if (mojo.internal.isMojomLoaded(mojomId)) {
    console.warn('The following mojom is loaded multiple times: ' + mojomId);
    return;
  }
  mojo.internal.markMojomLoaded(mojomId);
  var bindings = mojo;
  var associatedBindings = mojo;
  var codec = mojo.internal;
  var validator = mojo.internal;

  var exports = mojo.internal.exposeNamespace('device.mojom');
  var sensor$ =
      mojo.internal.exposeNamespace('device.mojom');
  if (mojo.config.autoLoadMojomDeps) {
    mojo.internal.loadMojomIfNecessary(
        'services/device/public/mojom/sensor.mojom', 'sensor.mojom.js');
  }


  var SensorCreationResult = {};
  SensorCreationResult.SUCCESS = 0;
  SensorCreationResult.ERROR_NOT_AVAILABLE = SensorCreationResult.SUCCESS + 1;
  SensorCreationResult.ERROR_NOT_ALLOWED = SensorCreationResult.ERROR_NOT_AVAILABLE + 1;

  SensorCreationResult.isKnownEnumValue = function(value) {
    switch (value) {
    case 0:
    case 1:
    case 2:
      return true;
    }
    return false;
  };

  SensorCreationResult.validate = function(enumValue) {
    var isExtensible = false;
    if (isExtensible || this.isKnownEnumValue(enumValue))
      return validator.validationError.NONE;

    return validator.validationError.UNKNOWN_ENUM_VALUE;
  };

  function SensorInitParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  SensorInitParams.kReadBufferSizeForTests = 48;
  SensorInitParams.prototype.initDefaults_ = function() {
    this.sensor = new sensor$.SensorPtr();
    this.clientRequest = new bindings.InterfaceRequest();
    this.memory = null;
    this.bufferOffset = 0;
    this.mode = 0;
    this.defaultConfiguration = null;
    this.maximumFrequency = 0;
    this.minimumFrequency = 0;
  };
  SensorInitParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  SensorInitParams.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 64}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;


    // validate SensorInitParams.sensor
    err = messageValidator.validateInterface(offset + codec.kStructHeaderSize + 0, false);
    if (err !== validator.validationError.NONE)
        return err;


    // validate SensorInitParams.clientRequest
    err = messageValidator.validateInterfaceRequest(offset + codec.kStructHeaderSize + 8, false)
    if (err !== validator.validationError.NONE)
        return err;


    // validate SensorInitParams.memory
    err = messageValidator.validateHandle(offset + codec.kStructHeaderSize + 12, false)
    if (err !== validator.validationError.NONE)
        return err;



    // validate SensorInitParams.mode
    err = messageValidator.validateEnum(offset + codec.kStructHeaderSize + 24, sensor$.ReportingMode);
    if (err !== validator.validationError.NONE)
        return err;


    // validate SensorInitParams.defaultConfiguration
    err = messageValidator.validateStructPointer(offset + codec.kStructHeaderSize + 32, sensor$.SensorConfiguration, false);
    if (err !== validator.validationError.NONE)
        return err;



    return validator.validationError.NONE;
  };

  SensorInitParams.encodedSize = codec.kStructHeaderSize + 56;

  SensorInitParams.decode = function(decoder) {
    var packed;
    var val = new SensorInitParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.sensor = decoder.decodeStruct(new codec.Interface(sensor$.SensorPtr));
    val.clientRequest = decoder.decodeStruct(codec.InterfaceRequest);
    val.memory = decoder.decodeStruct(codec.Handle);
    val.bufferOffset = decoder.decodeStruct(codec.Uint64);
    val.mode = decoder.decodeStruct(codec.Int32);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    val.defaultConfiguration = decoder.decodeStructPointer(sensor$.SensorConfiguration);
    val.maximumFrequency = decoder.decodeStruct(codec.Double);
    val.minimumFrequency = decoder.decodeStruct(codec.Double);
    return val;
  };

  SensorInitParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(SensorInitParams.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(new codec.Interface(sensor$.SensorPtr), val.sensor);
    encoder.encodeStruct(codec.InterfaceRequest, val.clientRequest);
    encoder.encodeStruct(codec.Handle, val.memory);
    encoder.encodeStruct(codec.Uint64, val.bufferOffset);
    encoder.encodeStruct(codec.Int32, val.mode);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.encodeStructPointer(sensor$.SensorConfiguration, val.defaultConfiguration);
    encoder.encodeStruct(codec.Double, val.maximumFrequency);
    encoder.encodeStruct(codec.Double, val.minimumFrequency);
  };
  function SensorProvider_GetSensor_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  SensorProvider_GetSensor_Params.prototype.initDefaults_ = function() {
    this.type = 0;
  };
  SensorProvider_GetSensor_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  SensorProvider_GetSensor_Params.validate = function(messageValidator, offset) {
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


    // validate SensorProvider_GetSensor_Params.type
    err = messageValidator.validateEnum(offset + codec.kStructHeaderSize + 0, sensor$.SensorType);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  SensorProvider_GetSensor_Params.encodedSize = codec.kStructHeaderSize + 8;

  SensorProvider_GetSensor_Params.decode = function(decoder) {
    var packed;
    var val = new SensorProvider_GetSensor_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.type = decoder.decodeStruct(codec.Int32);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    return val;
  };

  SensorProvider_GetSensor_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(SensorProvider_GetSensor_Params.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.Int32, val.type);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
  };
  function SensorProvider_GetSensor_ResponseParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  SensorProvider_GetSensor_ResponseParams.prototype.initDefaults_ = function() {
    this.result = 0;
    this.initParams = null;
  };
  SensorProvider_GetSensor_ResponseParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  SensorProvider_GetSensor_ResponseParams.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 24}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;


    // validate SensorProvider_GetSensor_ResponseParams.result
    err = messageValidator.validateEnum(offset + codec.kStructHeaderSize + 0, SensorCreationResult);
    if (err !== validator.validationError.NONE)
        return err;


    // validate SensorProvider_GetSensor_ResponseParams.initParams
    err = messageValidator.validateStructPointer(offset + codec.kStructHeaderSize + 8, SensorInitParams, true);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  SensorProvider_GetSensor_ResponseParams.encodedSize = codec.kStructHeaderSize + 16;

  SensorProvider_GetSensor_ResponseParams.decode = function(decoder) {
    var packed;
    var val = new SensorProvider_GetSensor_ResponseParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.result = decoder.decodeStruct(codec.Int32);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    val.initParams = decoder.decodeStructPointer(SensorInitParams);
    return val;
  };

  SensorProvider_GetSensor_ResponseParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(SensorProvider_GetSensor_ResponseParams.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.Int32, val.result);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.encodeStructPointer(SensorInitParams, val.initParams);
  };
  var kSensorProvider_GetSensor_Name = 0;

  function SensorProviderPtr(handleOrPtrInfo) {
    this.ptr = new bindings.InterfacePtrController(SensorProvider,
                                                   handleOrPtrInfo);
  }

  function SensorProviderAssociatedPtr(associatedInterfacePtrInfo) {
    this.ptr = new associatedBindings.AssociatedInterfacePtrController(
        SensorProvider, associatedInterfacePtrInfo);
  }

  SensorProviderAssociatedPtr.prototype =
      Object.create(SensorProviderPtr.prototype);
  SensorProviderAssociatedPtr.prototype.constructor =
      SensorProviderAssociatedPtr;

  function SensorProviderProxy(receiver) {
    this.receiver_ = receiver;
  }
  SensorProviderPtr.prototype.getSensor = function() {
    return SensorProviderProxy.prototype.getSensor
        .apply(this.ptr.getProxy(), arguments);
  };

  SensorProviderProxy.prototype.getSensor = function(type) {
    var params = new SensorProvider_GetSensor_Params();
    params.type = type;
    return new Promise(function(resolve, reject) {
      var builder = new codec.MessageV1Builder(
          kSensorProvider_GetSensor_Name,
          codec.align(SensorProvider_GetSensor_Params.encodedSize),
          codec.kMessageExpectsResponse, 0);
      builder.encodeStruct(SensorProvider_GetSensor_Params, params);
      var message = builder.finish();
      this.receiver_.acceptAndExpectResponse(message).then(function(message) {
        var reader = new codec.MessageReader(message);
        var responseParams =
            reader.decodeStruct(SensorProvider_GetSensor_ResponseParams);
        resolve(responseParams);
      }).catch(function(result) {
        reject(Error("Connection error: " + result));
      });
    }.bind(this));
  };

  function SensorProviderStub(delegate) {
    this.delegate_ = delegate;
  }
  SensorProviderStub.prototype.getSensor = function(type) {
    return this.delegate_ && this.delegate_.getSensor && this.delegate_.getSensor(type);
  }

  SensorProviderStub.prototype.accept = function(message) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    default:
      return false;
    }
  };

  SensorProviderStub.prototype.acceptWithResponder =
      function(message, responder) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    case kSensorProvider_GetSensor_Name:
      var params = reader.decodeStruct(SensorProvider_GetSensor_Params);
      this.getSensor(params.type).then(function(response) {
        var responseParams =
            new SensorProvider_GetSensor_ResponseParams();
        responseParams.result = response.result;
        responseParams.initParams = response.initParams;
        var builder = new codec.MessageV1Builder(
            kSensorProvider_GetSensor_Name,
            codec.align(SensorProvider_GetSensor_ResponseParams.encodedSize),
            codec.kMessageIsResponse, reader.requestID);
        builder.encodeStruct(SensorProvider_GetSensor_ResponseParams,
                             responseParams);
        var message = builder.finish();
        responder.accept(message);
      });
      return true;
    default:
      return false;
    }
  };

  function validateSensorProviderRequest(messageValidator) {
    var message = messageValidator.message;
    var paramsClass = null;
    switch (message.getName()) {
      case kSensorProvider_GetSensor_Name:
        if (message.expectsResponse())
          paramsClass = SensorProvider_GetSensor_Params;
      break;
    }
    if (paramsClass === null)
      return validator.validationError.NONE;
    return paramsClass.validate(messageValidator, messageValidator.message.getHeaderNumBytes());
  }

  function validateSensorProviderResponse(messageValidator) {
   var message = messageValidator.message;
   var paramsClass = null;
   switch (message.getName()) {
      case kSensorProvider_GetSensor_Name:
        if (message.isResponse())
          paramsClass = SensorProvider_GetSensor_ResponseParams;
        break;
    }
    if (paramsClass === null)
      return validator.validationError.NONE;
    return paramsClass.validate(messageValidator, messageValidator.message.getHeaderNumBytes());
  }

  var SensorProvider = {
    name: 'device.mojom.SensorProvider',
    kVersion: 0,
    ptrClass: SensorProviderPtr,
    proxyClass: SensorProviderProxy,
    stubClass: SensorProviderStub,
    validateRequest: validateSensorProviderRequest,
    validateResponse: validateSensorProviderResponse,
  };
  SensorProviderStub.prototype.validator = validateSensorProviderRequest;
  SensorProviderProxy.prototype.validator = validateSensorProviderResponse;
  exports.SensorCreationResult = SensorCreationResult;
  exports.SensorInitParams = SensorInitParams;
  exports.SensorProvider = SensorProvider;
  exports.SensorProviderPtr = SensorProviderPtr;
  exports.SensorProviderAssociatedPtr = SensorProviderAssociatedPtr;
})();
