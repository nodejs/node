// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {
  var mojomId = 'third_party/blink/public/mojom/usb/web_usb_service.mojom';
  if (mojo.internal.isMojomLoaded(mojomId)) {
    console.warn('The following mojom is loaded multiple times: ' + mojomId);
    return;
  }
  mojo.internal.markMojomLoaded(mojomId);
  var bindings = mojo;
  var associatedBindings = mojo;
  var codec = mojo.internal;
  var validator = mojo.internal;

  var exports = mojo.internal.exposeNamespace('blink.mojom');
  var device$ =
      mojo.internal.exposeNamespace('device.mojom');
  if (mojo.config.autoLoadMojomDeps) {
    mojo.internal.loadMojomIfNecessary(
        'device/usb/public/mojom/device.mojom', '../../../../../device/usb/public/mojom/device.mojom.js');
  }
  var device_manager$ =
      mojo.internal.exposeNamespace('device.mojom');
  if (mojo.config.autoLoadMojomDeps) {
    mojo.internal.loadMojomIfNecessary(
        'device/usb/public/mojom/device_manager.mojom', '../../../../../device/usb/public/mojom/device_manager.mojom.js');
  }



  function WebUsbService_GetDevices_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  WebUsbService_GetDevices_Params.prototype.initDefaults_ = function() {
  };
  WebUsbService_GetDevices_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };
  WebUsbService_GetDevices_Params.generate = function(generator_) {
    var generated = new WebUsbService_GetDevices_Params;
    return generated;
  };

  WebUsbService_GetDevices_Params.prototype.mutate = function(mutator_) {
    return this;
  };
  WebUsbService_GetDevices_Params.prototype.getHandleDeps = function() {
    var handles = [];
    return handles;
  };

  WebUsbService_GetDevices_Params.prototype.setHandles = function() {
    this.setHandlesInternal_(arguments, 0);
  };
  WebUsbService_GetDevices_Params.prototype.setHandlesInternal_ = function(handles, idx) {
    return idx;
  };

  WebUsbService_GetDevices_Params.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 8}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  WebUsbService_GetDevices_Params.encodedSize = codec.kStructHeaderSize + 0;

  WebUsbService_GetDevices_Params.decode = function(decoder) {
    var packed;
    var val = new WebUsbService_GetDevices_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  WebUsbService_GetDevices_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(WebUsbService_GetDevices_Params.encodedSize);
    encoder.writeUint32(0);
  };
  function WebUsbService_GetDevices_ResponseParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  WebUsbService_GetDevices_ResponseParams.prototype.initDefaults_ = function() {
    this.results = null;
  };
  WebUsbService_GetDevices_ResponseParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };
  WebUsbService_GetDevices_ResponseParams.generate = function(generator_) {
    var generated = new WebUsbService_GetDevices_ResponseParams;
    generated.results = generator_.generateArray(function() {
      return generator_.generateStruct(device.mojom.UsbDeviceInfo, false);
    });
    return generated;
  };

  WebUsbService_GetDevices_ResponseParams.prototype.mutate = function(mutator_) {
    if (mutator_.chooseMutateField()) {
      this.results = mutator_.mutateArray(this.results, function(val) {
        return mutator_.mutateStruct(val, device.mojom.UsbDeviceInfo, false);
      });
    }
    return this;
  };
  WebUsbService_GetDevices_ResponseParams.prototype.getHandleDeps = function() {
    var handles = [];
    return handles;
  };

  WebUsbService_GetDevices_ResponseParams.prototype.setHandles = function() {
    this.setHandlesInternal_(arguments, 0);
  };
  WebUsbService_GetDevices_ResponseParams.prototype.setHandlesInternal_ = function(handles, idx) {
    return idx;
  };

  WebUsbService_GetDevices_ResponseParams.validate = function(messageValidator, offset) {
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


    // validate WebUsbService_GetDevices_ResponseParams.results
    err = messageValidator.validateArrayPointer(offset + codec.kStructHeaderSize + 0, 8, new codec.PointerTo(device$.UsbDeviceInfo), false, [0], 0);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  WebUsbService_GetDevices_ResponseParams.encodedSize = codec.kStructHeaderSize + 8;

  WebUsbService_GetDevices_ResponseParams.decode = function(decoder) {
    var packed;
    var val = new WebUsbService_GetDevices_ResponseParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.results = decoder.decodeArrayPointer(new codec.PointerTo(device$.UsbDeviceInfo));
    return val;
  };

  WebUsbService_GetDevices_ResponseParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(WebUsbService_GetDevices_ResponseParams.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeArrayPointer(new codec.PointerTo(device$.UsbDeviceInfo), val.results);
  };
  function WebUsbService_GetDevice_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  WebUsbService_GetDevice_Params.prototype.initDefaults_ = function() {
    this.guid = null;
    this.deviceRequestd = new bindings.InterfaceRequest();
  };
  WebUsbService_GetDevice_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };
  WebUsbService_GetDevice_Params.generate = function(generator_) {
    var generated = new WebUsbService_GetDevice_Params;
    generated.guid = generator_.generateString(false);
    generated.deviceRequestd = generator_.generateInterfaceRequest("device.mojom.UsbDevice", false);
    return generated;
  };

  WebUsbService_GetDevice_Params.prototype.mutate = function(mutator_) {
    if (mutator_.chooseMutateField()) {
      this.guid = mutator_.mutateString(this.guid, false);
    }
    if (mutator_.chooseMutateField()) {
      this.deviceRequestd = mutator_.mutateInterfaceRequest(this.deviceRequestd, "device.mojom.UsbDevice", false);
    }
    return this;
  };
  WebUsbService_GetDevice_Params.prototype.getHandleDeps = function() {
    var handles = [];
    if (this.deviceRequestd !== null) {
      Array.prototype.push.apply(handles, ["device.mojom.UsbDeviceRequest"]);
    }
    return handles;
  };

  WebUsbService_GetDevice_Params.prototype.setHandles = function() {
    this.setHandlesInternal_(arguments, 0);
  };
  WebUsbService_GetDevice_Params.prototype.setHandlesInternal_ = function(handles, idx) {
    this.deviceRequestd = handles[idx++];;
    return idx;
  };

  WebUsbService_GetDevice_Params.validate = function(messageValidator, offset) {
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


    // validate WebUsbService_GetDevice_Params.guid
    err = messageValidator.validateStringPointer(offset + codec.kStructHeaderSize + 0, false)
    if (err !== validator.validationError.NONE)
        return err;


    // validate WebUsbService_GetDevice_Params.deviceRequestd
    err = messageValidator.validateInterfaceRequest(offset + codec.kStructHeaderSize + 8, false)
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  WebUsbService_GetDevice_Params.encodedSize = codec.kStructHeaderSize + 16;

  WebUsbService_GetDevice_Params.decode = function(decoder) {
    var packed;
    var val = new WebUsbService_GetDevice_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.guid = decoder.decodeStruct(codec.String);
    val.deviceRequestd = decoder.decodeStruct(codec.InterfaceRequest);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    return val;
  };

  WebUsbService_GetDevice_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(WebUsbService_GetDevice_Params.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.String, val.guid);
    encoder.encodeStruct(codec.InterfaceRequest, val.deviceRequestd);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
  };
  function WebUsbService_GetPermission_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  WebUsbService_GetPermission_Params.prototype.initDefaults_ = function() {
    this.deviceFilters = null;
  };
  WebUsbService_GetPermission_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };
  WebUsbService_GetPermission_Params.generate = function(generator_) {
    var generated = new WebUsbService_GetPermission_Params;
    generated.deviceFilters = generator_.generateArray(function() {
      return generator_.generateStruct(device.mojom.UsbDeviceFilter, false);
    });
    return generated;
  };

  WebUsbService_GetPermission_Params.prototype.mutate = function(mutator_) {
    if (mutator_.chooseMutateField()) {
      this.deviceFilters = mutator_.mutateArray(this.deviceFilters, function(val) {
        return mutator_.mutateStruct(val, device.mojom.UsbDeviceFilter, false);
      });
    }
    return this;
  };
  WebUsbService_GetPermission_Params.prototype.getHandleDeps = function() {
    var handles = [];
    return handles;
  };

  WebUsbService_GetPermission_Params.prototype.setHandles = function() {
    this.setHandlesInternal_(arguments, 0);
  };
  WebUsbService_GetPermission_Params.prototype.setHandlesInternal_ = function(handles, idx) {
    return idx;
  };

  WebUsbService_GetPermission_Params.validate = function(messageValidator, offset) {
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


    // validate WebUsbService_GetPermission_Params.deviceFilters
    err = messageValidator.validateArrayPointer(offset + codec.kStructHeaderSize + 0, 8, new codec.PointerTo(device_manager$.UsbDeviceFilter), false, [0], 0);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  WebUsbService_GetPermission_Params.encodedSize = codec.kStructHeaderSize + 8;

  WebUsbService_GetPermission_Params.decode = function(decoder) {
    var packed;
    var val = new WebUsbService_GetPermission_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.deviceFilters = decoder.decodeArrayPointer(new codec.PointerTo(device_manager$.UsbDeviceFilter));
    return val;
  };

  WebUsbService_GetPermission_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(WebUsbService_GetPermission_Params.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeArrayPointer(new codec.PointerTo(device_manager$.UsbDeviceFilter), val.deviceFilters);
  };
  function WebUsbService_GetPermission_ResponseParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  WebUsbService_GetPermission_ResponseParams.prototype.initDefaults_ = function() {
    this.result = null;
  };
  WebUsbService_GetPermission_ResponseParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };
  WebUsbService_GetPermission_ResponseParams.generate = function(generator_) {
    var generated = new WebUsbService_GetPermission_ResponseParams;
    generated.result = generator_.generateStruct(device.mojom.UsbDeviceInfo, true);
    return generated;
  };

  WebUsbService_GetPermission_ResponseParams.prototype.mutate = function(mutator_) {
    if (mutator_.chooseMutateField()) {
      this.result = mutator_.mutateStruct(this.result, device.mojom.UsbDeviceInfo, true);
    }
    return this;
  };
  WebUsbService_GetPermission_ResponseParams.prototype.getHandleDeps = function() {
    var handles = [];
    return handles;
  };

  WebUsbService_GetPermission_ResponseParams.prototype.setHandles = function() {
    this.setHandlesInternal_(arguments, 0);
  };
  WebUsbService_GetPermission_ResponseParams.prototype.setHandlesInternal_ = function(handles, idx) {
    return idx;
  };

  WebUsbService_GetPermission_ResponseParams.validate = function(messageValidator, offset) {
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


    // validate WebUsbService_GetPermission_ResponseParams.result
    err = messageValidator.validateStructPointer(offset + codec.kStructHeaderSize + 0, device$.UsbDeviceInfo, true);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  WebUsbService_GetPermission_ResponseParams.encodedSize = codec.kStructHeaderSize + 8;

  WebUsbService_GetPermission_ResponseParams.decode = function(decoder) {
    var packed;
    var val = new WebUsbService_GetPermission_ResponseParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.result = decoder.decodeStructPointer(device$.UsbDeviceInfo);
    return val;
  };

  WebUsbService_GetPermission_ResponseParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(WebUsbService_GetPermission_ResponseParams.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStructPointer(device$.UsbDeviceInfo, val.result);
  };
  function WebUsbService_SetClient_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  WebUsbService_SetClient_Params.prototype.initDefaults_ = function() {
    this.client = new device_manager$.UsbDeviceManagerClientPtr();
  };
  WebUsbService_SetClient_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };
  WebUsbService_SetClient_Params.generate = function(generator_) {
    var generated = new WebUsbService_SetClient_Params;
    generated.client = generator_.generateInterface("device.mojom.UsbDeviceManagerClient", false);
    return generated;
  };

  WebUsbService_SetClient_Params.prototype.mutate = function(mutator_) {
    if (mutator_.chooseMutateField()) {
      this.client = mutator_.mutateInterface(this.client, "device.mojom.UsbDeviceManagerClient", false);
    }
    return this;
  };
  WebUsbService_SetClient_Params.prototype.getHandleDeps = function() {
    var handles = [];
    if (this.client !== null) {
      Array.prototype.push.apply(handles, ["device.mojom.UsbDeviceManagerClientPtr"]);
    }
    return handles;
  };

  WebUsbService_SetClient_Params.prototype.setHandles = function() {
    this.setHandlesInternal_(arguments, 0);
  };
  WebUsbService_SetClient_Params.prototype.setHandlesInternal_ = function(handles, idx) {
    this.client = handles[idx++];;
    return idx;
  };

  WebUsbService_SetClient_Params.validate = function(messageValidator, offset) {
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


    // validate WebUsbService_SetClient_Params.client
    err = messageValidator.validateInterface(offset + codec.kStructHeaderSize + 0, false);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  WebUsbService_SetClient_Params.encodedSize = codec.kStructHeaderSize + 8;

  WebUsbService_SetClient_Params.decode = function(decoder) {
    var packed;
    var val = new WebUsbService_SetClient_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.client = decoder.decodeStruct(new codec.Interface(device_manager$.UsbDeviceManagerClientPtr));
    return val;
  };

  WebUsbService_SetClient_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(WebUsbService_SetClient_Params.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(new codec.Interface(device_manager$.UsbDeviceManagerClientPtr), val.client);
  };
  var kWebUsbService_GetDevices_Name = 0;
  var kWebUsbService_GetDevice_Name = 1;
  var kWebUsbService_GetPermission_Name = 2;
  var kWebUsbService_SetClient_Name = 3;

  function WebUsbServicePtr(handleOrPtrInfo) {
    this.ptr = new bindings.InterfacePtrController(WebUsbService,
                                                   handleOrPtrInfo);
  }

  function WebUsbServiceAssociatedPtr(associatedInterfacePtrInfo) {
    this.ptr = new associatedBindings.AssociatedInterfacePtrController(
        WebUsbService, associatedInterfacePtrInfo);
  }

  WebUsbServiceAssociatedPtr.prototype =
      Object.create(WebUsbServicePtr.prototype);
  WebUsbServiceAssociatedPtr.prototype.constructor =
      WebUsbServiceAssociatedPtr;

  function WebUsbServiceProxy(receiver) {
    this.receiver_ = receiver;
  }
  WebUsbServicePtr.prototype.getDevices = function() {
    return WebUsbServiceProxy.prototype.getDevices
        .apply(this.ptr.getProxy(), arguments);
  };

  WebUsbServiceProxy.prototype.getDevices = function() {
    var params_ = new WebUsbService_GetDevices_Params();
    return new Promise(function(resolve, reject) {
      var builder = new codec.MessageV1Builder(
          kWebUsbService_GetDevices_Name,
          codec.align(WebUsbService_GetDevices_Params.encodedSize),
          codec.kMessageExpectsResponse, 0);
      builder.encodeStruct(WebUsbService_GetDevices_Params, params_);
      var message = builder.finish();
      this.receiver_.acceptAndExpectResponse(message).then(function(message) {
        var reader = new codec.MessageReader(message);
        var responseParams =
            reader.decodeStruct(WebUsbService_GetDevices_ResponseParams);
        resolve(responseParams);
      }).catch(function(result) {
        reject(Error("Connection error: " + result));
      });
    }.bind(this));
  };
  WebUsbServicePtr.prototype.getDevice = function() {
    return WebUsbServiceProxy.prototype.getDevice
        .apply(this.ptr.getProxy(), arguments);
  };

  WebUsbServiceProxy.prototype.getDevice = function(guid, deviceRequestd) {
    var params_ = new WebUsbService_GetDevice_Params();
    params_.guid = guid;
    params_.deviceRequestd = deviceRequestd;
    var builder = new codec.MessageV0Builder(
        kWebUsbService_GetDevice_Name,
        codec.align(WebUsbService_GetDevice_Params.encodedSize));
    builder.encodeStruct(WebUsbService_GetDevice_Params, params_);
    var message = builder.finish();
    this.receiver_.accept(message);
  };
  WebUsbServicePtr.prototype.getPermission = function() {
    return WebUsbServiceProxy.prototype.getPermission
        .apply(this.ptr.getProxy(), arguments);
  };

  WebUsbServiceProxy.prototype.getPermission = function(deviceFilters) {
    var params_ = new WebUsbService_GetPermission_Params();
    params_.deviceFilters = deviceFilters;
    return new Promise(function(resolve, reject) {
      var builder = new codec.MessageV1Builder(
          kWebUsbService_GetPermission_Name,
          codec.align(WebUsbService_GetPermission_Params.encodedSize),
          codec.kMessageExpectsResponse, 0);
      builder.encodeStruct(WebUsbService_GetPermission_Params, params_);
      var message = builder.finish();
      this.receiver_.acceptAndExpectResponse(message).then(function(message) {
        var reader = new codec.MessageReader(message);
        var responseParams =
            reader.decodeStruct(WebUsbService_GetPermission_ResponseParams);
        resolve(responseParams);
      }).catch(function(result) {
        reject(Error("Connection error: " + result));
      });
    }.bind(this));
  };
  WebUsbServicePtr.prototype.setClient = function() {
    return WebUsbServiceProxy.prototype.setClient
        .apply(this.ptr.getProxy(), arguments);
  };

  WebUsbServiceProxy.prototype.setClient = function(client) {
    var params_ = new WebUsbService_SetClient_Params();
    params_.client = client;
    var builder = new codec.MessageV0Builder(
        kWebUsbService_SetClient_Name,
        codec.align(WebUsbService_SetClient_Params.encodedSize));
    builder.encodeStruct(WebUsbService_SetClient_Params, params_);
    var message = builder.finish();
    this.receiver_.accept(message);
  };

  function WebUsbServiceStub(delegate) {
    this.delegate_ = delegate;
  }
  WebUsbServiceStub.prototype.getDevices = function() {
    return this.delegate_ && this.delegate_.getDevices && this.delegate_.getDevices();
  }
  WebUsbServiceStub.prototype.getDevice = function(guid, deviceRequestd) {
    return this.delegate_ && this.delegate_.getDevice && this.delegate_.getDevice(guid, deviceRequestd);
  }
  WebUsbServiceStub.prototype.getPermission = function(deviceFilters) {
    return this.delegate_ && this.delegate_.getPermission && this.delegate_.getPermission(deviceFilters);
  }
  WebUsbServiceStub.prototype.setClient = function(client) {
    return this.delegate_ && this.delegate_.setClient && this.delegate_.setClient(client);
  }

  WebUsbServiceStub.prototype.accept = function(message) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    case kWebUsbService_GetDevice_Name:
      var params = reader.decodeStruct(WebUsbService_GetDevice_Params);
      this.getDevice(params.guid, params.deviceRequestd);
      return true;
    case kWebUsbService_SetClient_Name:
      var params = reader.decodeStruct(WebUsbService_SetClient_Params);
      this.setClient(params.client);
      return true;
    default:
      return false;
    }
  };

  WebUsbServiceStub.prototype.acceptWithResponder =
      function(message, responder) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    case kWebUsbService_GetDevices_Name:
      var params = reader.decodeStruct(WebUsbService_GetDevices_Params);
      this.getDevices().then(function(response) {
        var responseParams =
            new WebUsbService_GetDevices_ResponseParams();
        responseParams.results = response.results;
        var builder = new codec.MessageV1Builder(
            kWebUsbService_GetDevices_Name,
            codec.align(WebUsbService_GetDevices_ResponseParams.encodedSize),
            codec.kMessageIsResponse, reader.requestID);
        builder.encodeStruct(WebUsbService_GetDevices_ResponseParams,
                             responseParams);
        var message = builder.finish();
        responder.accept(message);
      });
      return true;
    case kWebUsbService_GetPermission_Name:
      var params = reader.decodeStruct(WebUsbService_GetPermission_Params);
      this.getPermission(params.deviceFilters).then(function(response) {
        var responseParams =
            new WebUsbService_GetPermission_ResponseParams();
        responseParams.result = response.result;
        var builder = new codec.MessageV1Builder(
            kWebUsbService_GetPermission_Name,
            codec.align(WebUsbService_GetPermission_ResponseParams.encodedSize),
            codec.kMessageIsResponse, reader.requestID);
        builder.encodeStruct(WebUsbService_GetPermission_ResponseParams,
                             responseParams);
        var message = builder.finish();
        responder.accept(message);
      });
      return true;
    default:
      return false;
    }
  };

  function validateWebUsbServiceRequest(messageValidator) {
    var message = messageValidator.message;
    var paramsClass = null;
    switch (message.getName()) {
      case kWebUsbService_GetDevices_Name:
        if (message.expectsResponse())
          paramsClass = WebUsbService_GetDevices_Params;
      break;
      case kWebUsbService_GetDevice_Name:
        if (!message.expectsResponse() && !message.isResponse())
          paramsClass = WebUsbService_GetDevice_Params;
      break;
      case kWebUsbService_GetPermission_Name:
        if (message.expectsResponse())
          paramsClass = WebUsbService_GetPermission_Params;
      break;
      case kWebUsbService_SetClient_Name:
        if (!message.expectsResponse() && !message.isResponse())
          paramsClass = WebUsbService_SetClient_Params;
      break;
    }
    if (paramsClass === null)
      return validator.validationError.NONE;
    return paramsClass.validate(messageValidator, messageValidator.message.getHeaderNumBytes());
  }

  function validateWebUsbServiceResponse(messageValidator) {
   var message = messageValidator.message;
   var paramsClass = null;
   switch (message.getName()) {
      case kWebUsbService_GetDevices_Name:
        if (message.isResponse())
          paramsClass = WebUsbService_GetDevices_ResponseParams;
        break;
      case kWebUsbService_GetPermission_Name:
        if (message.isResponse())
          paramsClass = WebUsbService_GetPermission_ResponseParams;
        break;
    }
    if (paramsClass === null)
      return validator.validationError.NONE;
    return paramsClass.validate(messageValidator, messageValidator.message.getHeaderNumBytes());
  }

  var WebUsbService = {
    name: 'blink.mojom.WebUsbService',
    kVersion: 0,
    ptrClass: WebUsbServicePtr,
    proxyClass: WebUsbServiceProxy,
    stubClass: WebUsbServiceStub,
    validateRequest: validateWebUsbServiceRequest,
    validateResponse: validateWebUsbServiceResponse,
    mojomId: 'third_party/blink/public/mojom/usb/web_usb_service.mojom',
    fuzzMethods: {
      getDevices: {
        params: WebUsbService_GetDevices_Params,
      },
      getDevice: {
        params: WebUsbService_GetDevice_Params,
      },
      getPermission: {
        params: WebUsbService_GetPermission_Params,
      },
      setClient: {
        params: WebUsbService_SetClient_Params,
      },
    },
  };
  WebUsbServiceStub.prototype.validator = validateWebUsbServiceRequest;
  WebUsbServiceProxy.prototype.validator = validateWebUsbServiceResponse;
  exports.WebUsbService = WebUsbService;
  exports.WebUsbServicePtr = WebUsbServicePtr;
  exports.WebUsbServiceAssociatedPtr = WebUsbServiceAssociatedPtr;
})();