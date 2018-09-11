// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {
  var mojomId = 'content/shell/common/layout_test/fake_bluetooth_chooser.mojom';
  if (mojo.internal.isMojomLoaded(mojomId)) {
    console.warn('The following mojom is loaded multiple times: ' + mojomId);
    return;
  }
  mojo.internal.markMojomLoaded(mojomId);
  var bindings = mojo;
  var associatedBindings = mojo;
  var codec = mojo.internal;
  var validator = mojo.internal;

  var exports = mojo.internal.exposeNamespace('content.mojom');


  var ChooserEventType = {};
  ChooserEventType.CHOOSER_OPENED = 0;
  ChooserEventType.SCAN_STARTED = ChooserEventType.CHOOSER_OPENED + 1;
  ChooserEventType.DEVICE_UPDATE = ChooserEventType.SCAN_STARTED + 1;
  ChooserEventType.ADAPTER_REMOVED = ChooserEventType.DEVICE_UPDATE + 1;
  ChooserEventType.ADAPTER_DISABLED = ChooserEventType.ADAPTER_REMOVED + 1;
  ChooserEventType.ADAPTER_ENABLED = ChooserEventType.ADAPTER_DISABLED + 1;
  ChooserEventType.DISCOVERY_FAILED_TO_START = ChooserEventType.ADAPTER_ENABLED + 1;
  ChooserEventType.DISCOVERING = ChooserEventType.DISCOVERY_FAILED_TO_START + 1;
  ChooserEventType.DISCOVERY_IDLE = ChooserEventType.DISCOVERING + 1;
  ChooserEventType.ADD_DEVICE = ChooserEventType.DISCOVERY_IDLE + 1;

  ChooserEventType.isKnownEnumValue = function(value) {
    switch (value) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
      return true;
    }
    return false;
  };

  ChooserEventType.validate = function(enumValue) {
    var isExtensible = false;
    if (isExtensible || this.isKnownEnumValue(enumValue))
      return validator.validationError.NONE;

    return validator.validationError.UNKNOWN_ENUM_VALUE;
  };

  function FakeBluetoothChooserEvent(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  FakeBluetoothChooserEvent.prototype.initDefaults_ = function() {
    this.type = 0;
    this.origin = null;
    this.peripheralAddress = null;
  };
  FakeBluetoothChooserEvent.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  FakeBluetoothChooserEvent.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 32}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;


    // validate FakeBluetoothChooserEvent.type
    err = messageValidator.validateEnum(offset + codec.kStructHeaderSize + 0, ChooserEventType);
    if (err !== validator.validationError.NONE)
        return err;


    // validate FakeBluetoothChooserEvent.origin
    err = messageValidator.validateStringPointer(offset + codec.kStructHeaderSize + 8, true)
    if (err !== validator.validationError.NONE)
        return err;


    // validate FakeBluetoothChooserEvent.peripheralAddress
    err = messageValidator.validateStringPointer(offset + codec.kStructHeaderSize + 16, true)
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  FakeBluetoothChooserEvent.encodedSize = codec.kStructHeaderSize + 24;

  FakeBluetoothChooserEvent.decode = function(decoder) {
    var packed;
    var val = new FakeBluetoothChooserEvent();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.type = decoder.decodeStruct(codec.Int32);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    val.origin = decoder.decodeStruct(codec.NullableString);
    val.peripheralAddress = decoder.decodeStruct(codec.NullableString);
    return val;
  };

  FakeBluetoothChooserEvent.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(FakeBluetoothChooserEvent.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.Int32, val.type);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.encodeStruct(codec.NullableString, val.origin);
    encoder.encodeStruct(codec.NullableString, val.peripheralAddress);
  };
  function FakeBluetoothChooser_WaitForEvents_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  FakeBluetoothChooser_WaitForEvents_Params.prototype.initDefaults_ = function() {
    this.numOfEvents = 0;
  };
  FakeBluetoothChooser_WaitForEvents_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  FakeBluetoothChooser_WaitForEvents_Params.validate = function(messageValidator, offset) {
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


    return validator.validationError.NONE;
  };

  FakeBluetoothChooser_WaitForEvents_Params.encodedSize = codec.kStructHeaderSize + 8;

  FakeBluetoothChooser_WaitForEvents_Params.decode = function(decoder) {
    var packed;
    var val = new FakeBluetoothChooser_WaitForEvents_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.numOfEvents = decoder.decodeStruct(codec.Uint32);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    return val;
  };

  FakeBluetoothChooser_WaitForEvents_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(FakeBluetoothChooser_WaitForEvents_Params.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.Uint32, val.numOfEvents);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
  };
  function FakeBluetoothChooser_WaitForEvents_ResponseParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  FakeBluetoothChooser_WaitForEvents_ResponseParams.prototype.initDefaults_ = function() {
    this.events = null;
  };
  FakeBluetoothChooser_WaitForEvents_ResponseParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  FakeBluetoothChooser_WaitForEvents_ResponseParams.validate = function(messageValidator, offset) {
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


    // validate FakeBluetoothChooser_WaitForEvents_ResponseParams.events
    err = messageValidator.validateArrayPointer(offset + codec.kStructHeaderSize + 0, 8, new codec.PointerTo(FakeBluetoothChooserEvent), false, [0], 0);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  FakeBluetoothChooser_WaitForEvents_ResponseParams.encodedSize = codec.kStructHeaderSize + 8;

  FakeBluetoothChooser_WaitForEvents_ResponseParams.decode = function(decoder) {
    var packed;
    var val = new FakeBluetoothChooser_WaitForEvents_ResponseParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.events = decoder.decodeArrayPointer(new codec.PointerTo(FakeBluetoothChooserEvent));
    return val;
  };

  FakeBluetoothChooser_WaitForEvents_ResponseParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(FakeBluetoothChooser_WaitForEvents_ResponseParams.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeArrayPointer(new codec.PointerTo(FakeBluetoothChooserEvent), val.events);
  };
  function FakeBluetoothChooser_SelectPeripheral_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  FakeBluetoothChooser_SelectPeripheral_Params.prototype.initDefaults_ = function() {
    this.peripheralAddress = null;
  };
  FakeBluetoothChooser_SelectPeripheral_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  FakeBluetoothChooser_SelectPeripheral_Params.validate = function(messageValidator, offset) {
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


    // validate FakeBluetoothChooser_SelectPeripheral_Params.peripheralAddress
    err = messageValidator.validateStringPointer(offset + codec.kStructHeaderSize + 0, false)
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  FakeBluetoothChooser_SelectPeripheral_Params.encodedSize = codec.kStructHeaderSize + 8;

  FakeBluetoothChooser_SelectPeripheral_Params.decode = function(decoder) {
    var packed;
    var val = new FakeBluetoothChooser_SelectPeripheral_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.peripheralAddress = decoder.decodeStruct(codec.String);
    return val;
  };

  FakeBluetoothChooser_SelectPeripheral_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(FakeBluetoothChooser_SelectPeripheral_Params.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.String, val.peripheralAddress);
  };
  function FakeBluetoothChooser_SelectPeripheral_ResponseParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  FakeBluetoothChooser_SelectPeripheral_ResponseParams.prototype.initDefaults_ = function() {
  };
  FakeBluetoothChooser_SelectPeripheral_ResponseParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  FakeBluetoothChooser_SelectPeripheral_ResponseParams.validate = function(messageValidator, offset) {
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

  FakeBluetoothChooser_SelectPeripheral_ResponseParams.encodedSize = codec.kStructHeaderSize + 0;

  FakeBluetoothChooser_SelectPeripheral_ResponseParams.decode = function(decoder) {
    var packed;
    var val = new FakeBluetoothChooser_SelectPeripheral_ResponseParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  FakeBluetoothChooser_SelectPeripheral_ResponseParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(FakeBluetoothChooser_SelectPeripheral_ResponseParams.encodedSize);
    encoder.writeUint32(0);
  };
  function FakeBluetoothChooser_Cancel_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  FakeBluetoothChooser_Cancel_Params.prototype.initDefaults_ = function() {
  };
  FakeBluetoothChooser_Cancel_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  FakeBluetoothChooser_Cancel_Params.validate = function(messageValidator, offset) {
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

  FakeBluetoothChooser_Cancel_Params.encodedSize = codec.kStructHeaderSize + 0;

  FakeBluetoothChooser_Cancel_Params.decode = function(decoder) {
    var packed;
    var val = new FakeBluetoothChooser_Cancel_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  FakeBluetoothChooser_Cancel_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(FakeBluetoothChooser_Cancel_Params.encodedSize);
    encoder.writeUint32(0);
  };
  function FakeBluetoothChooser_Cancel_ResponseParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  FakeBluetoothChooser_Cancel_ResponseParams.prototype.initDefaults_ = function() {
  };
  FakeBluetoothChooser_Cancel_ResponseParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  FakeBluetoothChooser_Cancel_ResponseParams.validate = function(messageValidator, offset) {
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

  FakeBluetoothChooser_Cancel_ResponseParams.encodedSize = codec.kStructHeaderSize + 0;

  FakeBluetoothChooser_Cancel_ResponseParams.decode = function(decoder) {
    var packed;
    var val = new FakeBluetoothChooser_Cancel_ResponseParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  FakeBluetoothChooser_Cancel_ResponseParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(FakeBluetoothChooser_Cancel_ResponseParams.encodedSize);
    encoder.writeUint32(0);
  };
  function FakeBluetoothChooser_Rescan_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  FakeBluetoothChooser_Rescan_Params.prototype.initDefaults_ = function() {
  };
  FakeBluetoothChooser_Rescan_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  FakeBluetoothChooser_Rescan_Params.validate = function(messageValidator, offset) {
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

  FakeBluetoothChooser_Rescan_Params.encodedSize = codec.kStructHeaderSize + 0;

  FakeBluetoothChooser_Rescan_Params.decode = function(decoder) {
    var packed;
    var val = new FakeBluetoothChooser_Rescan_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  FakeBluetoothChooser_Rescan_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(FakeBluetoothChooser_Rescan_Params.encodedSize);
    encoder.writeUint32(0);
  };
  function FakeBluetoothChooser_Rescan_ResponseParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  FakeBluetoothChooser_Rescan_ResponseParams.prototype.initDefaults_ = function() {
  };
  FakeBluetoothChooser_Rescan_ResponseParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  FakeBluetoothChooser_Rescan_ResponseParams.validate = function(messageValidator, offset) {
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

  FakeBluetoothChooser_Rescan_ResponseParams.encodedSize = codec.kStructHeaderSize + 0;

  FakeBluetoothChooser_Rescan_ResponseParams.decode = function(decoder) {
    var packed;
    var val = new FakeBluetoothChooser_Rescan_ResponseParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  FakeBluetoothChooser_Rescan_ResponseParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(FakeBluetoothChooser_Rescan_ResponseParams.encodedSize);
    encoder.writeUint32(0);
  };
  var kFakeBluetoothChooser_WaitForEvents_Name = 457051710;
  var kFakeBluetoothChooser_SelectPeripheral_Name = 1924310743;
  var kFakeBluetoothChooser_Cancel_Name = 1388880682;
  var kFakeBluetoothChooser_Rescan_Name = 2112671529;

  function FakeBluetoothChooserPtr(handleOrPtrInfo) {
    this.ptr = new bindings.InterfacePtrController(FakeBluetoothChooser,
                                                   handleOrPtrInfo);
  }

  function FakeBluetoothChooserAssociatedPtr(associatedInterfacePtrInfo) {
    this.ptr = new associatedBindings.AssociatedInterfacePtrController(
        FakeBluetoothChooser, associatedInterfacePtrInfo);
  }

  FakeBluetoothChooserAssociatedPtr.prototype =
      Object.create(FakeBluetoothChooserPtr.prototype);
  FakeBluetoothChooserAssociatedPtr.prototype.constructor =
      FakeBluetoothChooserAssociatedPtr;

  function FakeBluetoothChooserProxy(receiver) {
    this.receiver_ = receiver;
  }
  FakeBluetoothChooserPtr.prototype.waitForEvents = function() {
    return FakeBluetoothChooserProxy.prototype.waitForEvents
        .apply(this.ptr.getProxy(), arguments);
  };

  FakeBluetoothChooserProxy.prototype.waitForEvents = function(numOfEvents) {
    var params = new FakeBluetoothChooser_WaitForEvents_Params();
    params.numOfEvents = numOfEvents;
    return new Promise(function(resolve, reject) {
      var builder = new codec.MessageV1Builder(
          kFakeBluetoothChooser_WaitForEvents_Name,
          codec.align(FakeBluetoothChooser_WaitForEvents_Params.encodedSize),
          codec.kMessageExpectsResponse, 0);
      builder.encodeStruct(FakeBluetoothChooser_WaitForEvents_Params, params);
      var message = builder.finish();
      this.receiver_.acceptAndExpectResponse(message).then(function(message) {
        var reader = new codec.MessageReader(message);
        var responseParams =
            reader.decodeStruct(FakeBluetoothChooser_WaitForEvents_ResponseParams);
        resolve(responseParams);
      }).catch(function(result) {
        reject(Error("Connection error: " + result));
      });
    }.bind(this));
  };
  FakeBluetoothChooserPtr.prototype.selectPeripheral = function() {
    return FakeBluetoothChooserProxy.prototype.selectPeripheral
        .apply(this.ptr.getProxy(), arguments);
  };

  FakeBluetoothChooserProxy.prototype.selectPeripheral = function(peripheralAddress) {
    var params = new FakeBluetoothChooser_SelectPeripheral_Params();
    params.peripheralAddress = peripheralAddress;
    return new Promise(function(resolve, reject) {
      var builder = new codec.MessageV1Builder(
          kFakeBluetoothChooser_SelectPeripheral_Name,
          codec.align(FakeBluetoothChooser_SelectPeripheral_Params.encodedSize),
          codec.kMessageExpectsResponse, 0);
      builder.encodeStruct(FakeBluetoothChooser_SelectPeripheral_Params, params);
      var message = builder.finish();
      this.receiver_.acceptAndExpectResponse(message).then(function(message) {
        var reader = new codec.MessageReader(message);
        var responseParams =
            reader.decodeStruct(FakeBluetoothChooser_SelectPeripheral_ResponseParams);
        resolve(responseParams);
      }).catch(function(result) {
        reject(Error("Connection error: " + result));
      });
    }.bind(this));
  };
  FakeBluetoothChooserPtr.prototype.cancel = function() {
    return FakeBluetoothChooserProxy.prototype.cancel
        .apply(this.ptr.getProxy(), arguments);
  };

  FakeBluetoothChooserProxy.prototype.cancel = function() {
    var params = new FakeBluetoothChooser_Cancel_Params();
    return new Promise(function(resolve, reject) {
      var builder = new codec.MessageV1Builder(
          kFakeBluetoothChooser_Cancel_Name,
          codec.align(FakeBluetoothChooser_Cancel_Params.encodedSize),
          codec.kMessageExpectsResponse, 0);
      builder.encodeStruct(FakeBluetoothChooser_Cancel_Params, params);
      var message = builder.finish();
      this.receiver_.acceptAndExpectResponse(message).then(function(message) {
        var reader = new codec.MessageReader(message);
        var responseParams =
            reader.decodeStruct(FakeBluetoothChooser_Cancel_ResponseParams);
        resolve(responseParams);
      }).catch(function(result) {
        reject(Error("Connection error: " + result));
      });
    }.bind(this));
  };
  FakeBluetoothChooserPtr.prototype.rescan = function() {
    return FakeBluetoothChooserProxy.prototype.rescan
        .apply(this.ptr.getProxy(), arguments);
  };

  FakeBluetoothChooserProxy.prototype.rescan = function() {
    var params = new FakeBluetoothChooser_Rescan_Params();
    return new Promise(function(resolve, reject) {
      var builder = new codec.MessageV1Builder(
          kFakeBluetoothChooser_Rescan_Name,
          codec.align(FakeBluetoothChooser_Rescan_Params.encodedSize),
          codec.kMessageExpectsResponse, 0);
      builder.encodeStruct(FakeBluetoothChooser_Rescan_Params, params);
      var message = builder.finish();
      this.receiver_.acceptAndExpectResponse(message).then(function(message) {
        var reader = new codec.MessageReader(message);
        var responseParams =
            reader.decodeStruct(FakeBluetoothChooser_Rescan_ResponseParams);
        resolve(responseParams);
      }).catch(function(result) {
        reject(Error("Connection error: " + result));
      });
    }.bind(this));
  };

  function FakeBluetoothChooserStub(delegate) {
    this.delegate_ = delegate;
  }
  FakeBluetoothChooserStub.prototype.waitForEvents = function(numOfEvents) {
    return this.delegate_ && this.delegate_.waitForEvents && this.delegate_.waitForEvents(numOfEvents);
  }
  FakeBluetoothChooserStub.prototype.selectPeripheral = function(peripheralAddress) {
    return this.delegate_ && this.delegate_.selectPeripheral && this.delegate_.selectPeripheral(peripheralAddress);
  }
  FakeBluetoothChooserStub.prototype.cancel = function() {
    return this.delegate_ && this.delegate_.cancel && this.delegate_.cancel();
  }
  FakeBluetoothChooserStub.prototype.rescan = function() {
    return this.delegate_ && this.delegate_.rescan && this.delegate_.rescan();
  }

  FakeBluetoothChooserStub.prototype.accept = function(message) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    default:
      return false;
    }
  };

  FakeBluetoothChooserStub.prototype.acceptWithResponder =
      function(message, responder) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    case kFakeBluetoothChooser_WaitForEvents_Name:
      var params = reader.decodeStruct(FakeBluetoothChooser_WaitForEvents_Params);
      this.waitForEvents(params.numOfEvents).then(function(response) {
        var responseParams =
            new FakeBluetoothChooser_WaitForEvents_ResponseParams();
        responseParams.events = response.events;
        var builder = new codec.MessageV1Builder(
            kFakeBluetoothChooser_WaitForEvents_Name,
            codec.align(FakeBluetoothChooser_WaitForEvents_ResponseParams.encodedSize),
            codec.kMessageIsResponse, reader.requestID);
        builder.encodeStruct(FakeBluetoothChooser_WaitForEvents_ResponseParams,
                             responseParams);
        var message = builder.finish();
        responder.accept(message);
      });
      return true;
    case kFakeBluetoothChooser_SelectPeripheral_Name:
      var params = reader.decodeStruct(FakeBluetoothChooser_SelectPeripheral_Params);
      this.selectPeripheral(params.peripheralAddress).then(function(response) {
        var responseParams =
            new FakeBluetoothChooser_SelectPeripheral_ResponseParams();
        var builder = new codec.MessageV1Builder(
            kFakeBluetoothChooser_SelectPeripheral_Name,
            codec.align(FakeBluetoothChooser_SelectPeripheral_ResponseParams.encodedSize),
            codec.kMessageIsResponse, reader.requestID);
        builder.encodeStruct(FakeBluetoothChooser_SelectPeripheral_ResponseParams,
                             responseParams);
        var message = builder.finish();
        responder.accept(message);
      });
      return true;
    case kFakeBluetoothChooser_Cancel_Name:
      var params = reader.decodeStruct(FakeBluetoothChooser_Cancel_Params);
      this.cancel().then(function(response) {
        var responseParams =
            new FakeBluetoothChooser_Cancel_ResponseParams();
        var builder = new codec.MessageV1Builder(
            kFakeBluetoothChooser_Cancel_Name,
            codec.align(FakeBluetoothChooser_Cancel_ResponseParams.encodedSize),
            codec.kMessageIsResponse, reader.requestID);
        builder.encodeStruct(FakeBluetoothChooser_Cancel_ResponseParams,
                             responseParams);
        var message = builder.finish();
        responder.accept(message);
      });
      return true;
    case kFakeBluetoothChooser_Rescan_Name:
      var params = reader.decodeStruct(FakeBluetoothChooser_Rescan_Params);
      this.rescan().then(function(response) {
        var responseParams =
            new FakeBluetoothChooser_Rescan_ResponseParams();
        var builder = new codec.MessageV1Builder(
            kFakeBluetoothChooser_Rescan_Name,
            codec.align(FakeBluetoothChooser_Rescan_ResponseParams.encodedSize),
            codec.kMessageIsResponse, reader.requestID);
        builder.encodeStruct(FakeBluetoothChooser_Rescan_ResponseParams,
                             responseParams);
        var message = builder.finish();
        responder.accept(message);
      });
      return true;
    default:
      return false;
    }
  };

  function validateFakeBluetoothChooserRequest(messageValidator) {
    var message = messageValidator.message;
    var paramsClass = null;
    switch (message.getName()) {
      case kFakeBluetoothChooser_WaitForEvents_Name:
        if (message.expectsResponse())
          paramsClass = FakeBluetoothChooser_WaitForEvents_Params;
      break;
      case kFakeBluetoothChooser_SelectPeripheral_Name:
        if (message.expectsResponse())
          paramsClass = FakeBluetoothChooser_SelectPeripheral_Params;
      break;
      case kFakeBluetoothChooser_Cancel_Name:
        if (message.expectsResponse())
          paramsClass = FakeBluetoothChooser_Cancel_Params;
      break;
      case kFakeBluetoothChooser_Rescan_Name:
        if (message.expectsResponse())
          paramsClass = FakeBluetoothChooser_Rescan_Params;
      break;
    }
    if (paramsClass === null)
      return validator.validationError.NONE;
    return paramsClass.validate(messageValidator, messageValidator.message.getHeaderNumBytes());
  }

  function validateFakeBluetoothChooserResponse(messageValidator) {
   var message = messageValidator.message;
   var paramsClass = null;
   switch (message.getName()) {
      case kFakeBluetoothChooser_WaitForEvents_Name:
        if (message.isResponse())
          paramsClass = FakeBluetoothChooser_WaitForEvents_ResponseParams;
        break;
      case kFakeBluetoothChooser_SelectPeripheral_Name:
        if (message.isResponse())
          paramsClass = FakeBluetoothChooser_SelectPeripheral_ResponseParams;
        break;
      case kFakeBluetoothChooser_Cancel_Name:
        if (message.isResponse())
          paramsClass = FakeBluetoothChooser_Cancel_ResponseParams;
        break;
      case kFakeBluetoothChooser_Rescan_Name:
        if (message.isResponse())
          paramsClass = FakeBluetoothChooser_Rescan_ResponseParams;
        break;
    }
    if (paramsClass === null)
      return validator.validationError.NONE;
    return paramsClass.validate(messageValidator, messageValidator.message.getHeaderNumBytes());
  }

  var FakeBluetoothChooser = {
    name: 'content.mojom.FakeBluetoothChooser',
    kVersion: 0,
    ptrClass: FakeBluetoothChooserPtr,
    proxyClass: FakeBluetoothChooserProxy,
    stubClass: FakeBluetoothChooserStub,
    validateRequest: validateFakeBluetoothChooserRequest,
    validateResponse: validateFakeBluetoothChooserResponse,
  };
  FakeBluetoothChooserStub.prototype.validator = validateFakeBluetoothChooserRequest;
  FakeBluetoothChooserProxy.prototype.validator = validateFakeBluetoothChooserResponse;
  exports.ChooserEventType = ChooserEventType;
  exports.FakeBluetoothChooserEvent = FakeBluetoothChooserEvent;
  exports.FakeBluetoothChooser = FakeBluetoothChooser;
  exports.FakeBluetoothChooserPtr = FakeBluetoothChooserPtr;
  exports.FakeBluetoothChooserAssociatedPtr = FakeBluetoothChooserAssociatedPtr;
})();
