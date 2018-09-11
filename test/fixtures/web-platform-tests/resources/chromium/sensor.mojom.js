// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {
  var mojomId = 'services/device/public/mojom/sensor.mojom';
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


  var SensorType = {};
  SensorType.FIRST = 1;
  SensorType.AMBIENT_LIGHT = SensorType.FIRST;
  SensorType.PROXIMITY = SensorType.AMBIENT_LIGHT + 1;
  SensorType.ACCELEROMETER = SensorType.PROXIMITY + 1;
  SensorType.LINEAR_ACCELERATION = SensorType.ACCELEROMETER + 1;
  SensorType.GYROSCOPE = SensorType.LINEAR_ACCELERATION + 1;
  SensorType.MAGNETOMETER = SensorType.GYROSCOPE + 1;
  SensorType.PRESSURE = SensorType.MAGNETOMETER + 1;
  SensorType.ABSOLUTE_ORIENTATION_EULER_ANGLES = SensorType.PRESSURE + 1;
  SensorType.ABSOLUTE_ORIENTATION_QUATERNION = SensorType.ABSOLUTE_ORIENTATION_EULER_ANGLES + 1;
  SensorType.RELATIVE_ORIENTATION_EULER_ANGLES = SensorType.ABSOLUTE_ORIENTATION_QUATERNION + 1;
  SensorType.RELATIVE_ORIENTATION_QUATERNION = SensorType.RELATIVE_ORIENTATION_EULER_ANGLES + 1;
  SensorType.LAST = SensorType.RELATIVE_ORIENTATION_QUATERNION;

  SensorType.isKnownEnumValue = function(value) {
    switch (value) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
      return true;
    }
    return false;
  };

  SensorType.validate = function(enumValue) {
    var isExtensible = false;
    if (isExtensible || this.isKnownEnumValue(enumValue))
      return validator.validationError.NONE;

    return validator.validationError.UNKNOWN_ENUM_VALUE;
  };
  var ReportingMode = {};
  ReportingMode.ON_CHANGE = 0;
  ReportingMode.CONTINUOUS = ReportingMode.ON_CHANGE + 1;

  ReportingMode.isKnownEnumValue = function(value) {
    switch (value) {
    case 0:
    case 1:
      return true;
    }
    return false;
  };

  ReportingMode.validate = function(enumValue) {
    var isExtensible = false;
    if (isExtensible || this.isKnownEnumValue(enumValue))
      return validator.validationError.NONE;

    return validator.validationError.UNKNOWN_ENUM_VALUE;
  };

  function SensorConfiguration(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  SensorConfiguration.prototype.initDefaults_ = function() {
    this.frequency = 0;
  };
  SensorConfiguration.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  SensorConfiguration.validate = function(messageValidator, offset) {
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

  SensorConfiguration.encodedSize = codec.kStructHeaderSize + 8;

  SensorConfiguration.decode = function(decoder) {
    var packed;
    var val = new SensorConfiguration();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.frequency = decoder.decodeStruct(codec.Double);
    return val;
  };

  SensorConfiguration.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(SensorConfiguration.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.Double, val.frequency);
  };
  function Sensor_GetDefaultConfiguration_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  Sensor_GetDefaultConfiguration_Params.prototype.initDefaults_ = function() {
  };
  Sensor_GetDefaultConfiguration_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  Sensor_GetDefaultConfiguration_Params.validate = function(messageValidator, offset) {
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

  Sensor_GetDefaultConfiguration_Params.encodedSize = codec.kStructHeaderSize + 0;

  Sensor_GetDefaultConfiguration_Params.decode = function(decoder) {
    var packed;
    var val = new Sensor_GetDefaultConfiguration_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  Sensor_GetDefaultConfiguration_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(Sensor_GetDefaultConfiguration_Params.encodedSize);
    encoder.writeUint32(0);
  };
  function Sensor_GetDefaultConfiguration_ResponseParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  Sensor_GetDefaultConfiguration_ResponseParams.prototype.initDefaults_ = function() {
    this.configuration = null;
  };
  Sensor_GetDefaultConfiguration_ResponseParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  Sensor_GetDefaultConfiguration_ResponseParams.validate = function(messageValidator, offset) {
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


    // validate Sensor_GetDefaultConfiguration_ResponseParams.configuration
    err = messageValidator.validateStructPointer(offset + codec.kStructHeaderSize + 0, SensorConfiguration, false);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  Sensor_GetDefaultConfiguration_ResponseParams.encodedSize = codec.kStructHeaderSize + 8;

  Sensor_GetDefaultConfiguration_ResponseParams.decode = function(decoder) {
    var packed;
    var val = new Sensor_GetDefaultConfiguration_ResponseParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.configuration = decoder.decodeStructPointer(SensorConfiguration);
    return val;
  };

  Sensor_GetDefaultConfiguration_ResponseParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(Sensor_GetDefaultConfiguration_ResponseParams.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStructPointer(SensorConfiguration, val.configuration);
  };
  function Sensor_AddConfiguration_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  Sensor_AddConfiguration_Params.prototype.initDefaults_ = function() {
    this.configuration = null;
  };
  Sensor_AddConfiguration_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  Sensor_AddConfiguration_Params.validate = function(messageValidator, offset) {
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


    // validate Sensor_AddConfiguration_Params.configuration
    err = messageValidator.validateStructPointer(offset + codec.kStructHeaderSize + 0, SensorConfiguration, false);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  Sensor_AddConfiguration_Params.encodedSize = codec.kStructHeaderSize + 8;

  Sensor_AddConfiguration_Params.decode = function(decoder) {
    var packed;
    var val = new Sensor_AddConfiguration_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.configuration = decoder.decodeStructPointer(SensorConfiguration);
    return val;
  };

  Sensor_AddConfiguration_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(Sensor_AddConfiguration_Params.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStructPointer(SensorConfiguration, val.configuration);
  };
  function Sensor_AddConfiguration_ResponseParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  Sensor_AddConfiguration_ResponseParams.prototype.initDefaults_ = function() {
    this.success = false;
  };
  Sensor_AddConfiguration_ResponseParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  Sensor_AddConfiguration_ResponseParams.validate = function(messageValidator, offset) {
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

  Sensor_AddConfiguration_ResponseParams.encodedSize = codec.kStructHeaderSize + 8;

  Sensor_AddConfiguration_ResponseParams.decode = function(decoder) {
    var packed;
    var val = new Sensor_AddConfiguration_ResponseParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    packed = decoder.readUint8();
    val.success = (packed >> 0) & 1 ? true : false;
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    return val;
  };

  Sensor_AddConfiguration_ResponseParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(Sensor_AddConfiguration_ResponseParams.encodedSize);
    encoder.writeUint32(0);
    packed = 0;
    packed |= (val.success & 1) << 0
    encoder.writeUint8(packed);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
  };
  function Sensor_RemoveConfiguration_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  Sensor_RemoveConfiguration_Params.prototype.initDefaults_ = function() {
    this.configuration = null;
  };
  Sensor_RemoveConfiguration_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  Sensor_RemoveConfiguration_Params.validate = function(messageValidator, offset) {
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


    // validate Sensor_RemoveConfiguration_Params.configuration
    err = messageValidator.validateStructPointer(offset + codec.kStructHeaderSize + 0, SensorConfiguration, false);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  Sensor_RemoveConfiguration_Params.encodedSize = codec.kStructHeaderSize + 8;

  Sensor_RemoveConfiguration_Params.decode = function(decoder) {
    var packed;
    var val = new Sensor_RemoveConfiguration_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.configuration = decoder.decodeStructPointer(SensorConfiguration);
    return val;
  };

  Sensor_RemoveConfiguration_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(Sensor_RemoveConfiguration_Params.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStructPointer(SensorConfiguration, val.configuration);
  };
  function Sensor_Suspend_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  Sensor_Suspend_Params.prototype.initDefaults_ = function() {
  };
  Sensor_Suspend_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  Sensor_Suspend_Params.validate = function(messageValidator, offset) {
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

  Sensor_Suspend_Params.encodedSize = codec.kStructHeaderSize + 0;

  Sensor_Suspend_Params.decode = function(decoder) {
    var packed;
    var val = new Sensor_Suspend_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  Sensor_Suspend_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(Sensor_Suspend_Params.encodedSize);
    encoder.writeUint32(0);
  };
  function Sensor_Resume_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  Sensor_Resume_Params.prototype.initDefaults_ = function() {
  };
  Sensor_Resume_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  Sensor_Resume_Params.validate = function(messageValidator, offset) {
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

  Sensor_Resume_Params.encodedSize = codec.kStructHeaderSize + 0;

  Sensor_Resume_Params.decode = function(decoder) {
    var packed;
    var val = new Sensor_Resume_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  Sensor_Resume_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(Sensor_Resume_Params.encodedSize);
    encoder.writeUint32(0);
  };
  function Sensor_ConfigureReadingChangeNotifications_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  Sensor_ConfigureReadingChangeNotifications_Params.prototype.initDefaults_ = function() {
    this.enabled = false;
  };
  Sensor_ConfigureReadingChangeNotifications_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  Sensor_ConfigureReadingChangeNotifications_Params.validate = function(messageValidator, offset) {
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

  Sensor_ConfigureReadingChangeNotifications_Params.encodedSize = codec.kStructHeaderSize + 8;

  Sensor_ConfigureReadingChangeNotifications_Params.decode = function(decoder) {
    var packed;
    var val = new Sensor_ConfigureReadingChangeNotifications_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    packed = decoder.readUint8();
    val.enabled = (packed >> 0) & 1 ? true : false;
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    return val;
  };

  Sensor_ConfigureReadingChangeNotifications_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(Sensor_ConfigureReadingChangeNotifications_Params.encodedSize);
    encoder.writeUint32(0);
    packed = 0;
    packed |= (val.enabled & 1) << 0
    encoder.writeUint8(packed);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
  };
  function SensorClient_RaiseError_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  SensorClient_RaiseError_Params.prototype.initDefaults_ = function() {
  };
  SensorClient_RaiseError_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  SensorClient_RaiseError_Params.validate = function(messageValidator, offset) {
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

  SensorClient_RaiseError_Params.encodedSize = codec.kStructHeaderSize + 0;

  SensorClient_RaiseError_Params.decode = function(decoder) {
    var packed;
    var val = new SensorClient_RaiseError_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  SensorClient_RaiseError_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(SensorClient_RaiseError_Params.encodedSize);
    encoder.writeUint32(0);
  };
  function SensorClient_SensorReadingChanged_Params(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  SensorClient_SensorReadingChanged_Params.prototype.initDefaults_ = function() {
  };
  SensorClient_SensorReadingChanged_Params.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  SensorClient_SensorReadingChanged_Params.validate = function(messageValidator, offset) {
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

  SensorClient_SensorReadingChanged_Params.encodedSize = codec.kStructHeaderSize + 0;

  SensorClient_SensorReadingChanged_Params.decode = function(decoder) {
    var packed;
    var val = new SensorClient_SensorReadingChanged_Params();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  SensorClient_SensorReadingChanged_Params.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(SensorClient_SensorReadingChanged_Params.encodedSize);
    encoder.writeUint32(0);
  };
  var kSensor_GetDefaultConfiguration_Name = 0;
  var kSensor_AddConfiguration_Name = 1;
  var kSensor_RemoveConfiguration_Name = 2;
  var kSensor_Suspend_Name = 3;
  var kSensor_Resume_Name = 4;
  var kSensor_ConfigureReadingChangeNotifications_Name = 5;

  function SensorPtr(handleOrPtrInfo) {
    this.ptr = new bindings.InterfacePtrController(Sensor,
                                                   handleOrPtrInfo);
  }

  function SensorAssociatedPtr(associatedInterfacePtrInfo) {
    this.ptr = new associatedBindings.AssociatedInterfacePtrController(
        Sensor, associatedInterfacePtrInfo);
  }

  SensorAssociatedPtr.prototype =
      Object.create(SensorPtr.prototype);
  SensorAssociatedPtr.prototype.constructor =
      SensorAssociatedPtr;

  function SensorProxy(receiver) {
    this.receiver_ = receiver;
  }
  SensorPtr.prototype.getDefaultConfiguration = function() {
    return SensorProxy.prototype.getDefaultConfiguration
        .apply(this.ptr.getProxy(), arguments);
  };

  SensorProxy.prototype.getDefaultConfiguration = function() {
    var params = new Sensor_GetDefaultConfiguration_Params();
    return new Promise(function(resolve, reject) {
      var builder = new codec.MessageV1Builder(
          kSensor_GetDefaultConfiguration_Name,
          codec.align(Sensor_GetDefaultConfiguration_Params.encodedSize),
          codec.kMessageExpectsResponse, 0);
      builder.encodeStruct(Sensor_GetDefaultConfiguration_Params, params);
      var message = builder.finish();
      this.receiver_.acceptAndExpectResponse(message).then(function(message) {
        var reader = new codec.MessageReader(message);
        var responseParams =
            reader.decodeStruct(Sensor_GetDefaultConfiguration_ResponseParams);
        resolve(responseParams);
      }).catch(function(result) {
        reject(Error("Connection error: " + result));
      });
    }.bind(this));
  };
  SensorPtr.prototype.addConfiguration = function() {
    return SensorProxy.prototype.addConfiguration
        .apply(this.ptr.getProxy(), arguments);
  };

  SensorProxy.prototype.addConfiguration = function(configuration) {
    var params = new Sensor_AddConfiguration_Params();
    params.configuration = configuration;
    return new Promise(function(resolve, reject) {
      var builder = new codec.MessageV1Builder(
          kSensor_AddConfiguration_Name,
          codec.align(Sensor_AddConfiguration_Params.encodedSize),
          codec.kMessageExpectsResponse, 0);
      builder.encodeStruct(Sensor_AddConfiguration_Params, params);
      var message = builder.finish();
      this.receiver_.acceptAndExpectResponse(message).then(function(message) {
        var reader = new codec.MessageReader(message);
        var responseParams =
            reader.decodeStruct(Sensor_AddConfiguration_ResponseParams);
        resolve(responseParams);
      }).catch(function(result) {
        reject(Error("Connection error: " + result));
      });
    }.bind(this));
  };
  SensorPtr.prototype.removeConfiguration = function() {
    return SensorProxy.prototype.removeConfiguration
        .apply(this.ptr.getProxy(), arguments);
  };

  SensorProxy.prototype.removeConfiguration = function(configuration) {
    var params = new Sensor_RemoveConfiguration_Params();
    params.configuration = configuration;
    var builder = new codec.MessageV0Builder(
        kSensor_RemoveConfiguration_Name,
        codec.align(Sensor_RemoveConfiguration_Params.encodedSize));
    builder.encodeStruct(Sensor_RemoveConfiguration_Params, params);
    var message = builder.finish();
    this.receiver_.accept(message);
  };
  SensorPtr.prototype.suspend = function() {
    return SensorProxy.prototype.suspend
        .apply(this.ptr.getProxy(), arguments);
  };

  SensorProxy.prototype.suspend = function() {
    var params = new Sensor_Suspend_Params();
    var builder = new codec.MessageV0Builder(
        kSensor_Suspend_Name,
        codec.align(Sensor_Suspend_Params.encodedSize));
    builder.encodeStruct(Sensor_Suspend_Params, params);
    var message = builder.finish();
    this.receiver_.accept(message);
  };
  SensorPtr.prototype.resume = function() {
    return SensorProxy.prototype.resume
        .apply(this.ptr.getProxy(), arguments);
  };

  SensorProxy.prototype.resume = function() {
    var params = new Sensor_Resume_Params();
    var builder = new codec.MessageV0Builder(
        kSensor_Resume_Name,
        codec.align(Sensor_Resume_Params.encodedSize));
    builder.encodeStruct(Sensor_Resume_Params, params);
    var message = builder.finish();
    this.receiver_.accept(message);
  };
  SensorPtr.prototype.configureReadingChangeNotifications = function() {
    return SensorProxy.prototype.configureReadingChangeNotifications
        .apply(this.ptr.getProxy(), arguments);
  };

  SensorProxy.prototype.configureReadingChangeNotifications = function(enabled) {
    var params = new Sensor_ConfigureReadingChangeNotifications_Params();
    params.enabled = enabled;
    var builder = new codec.MessageV0Builder(
        kSensor_ConfigureReadingChangeNotifications_Name,
        codec.align(Sensor_ConfigureReadingChangeNotifications_Params.encodedSize));
    builder.encodeStruct(Sensor_ConfigureReadingChangeNotifications_Params, params);
    var message = builder.finish();
    this.receiver_.accept(message);
  };

  function SensorStub(delegate) {
    this.delegate_ = delegate;
  }
  SensorStub.prototype.getDefaultConfiguration = function() {
    return this.delegate_ && this.delegate_.getDefaultConfiguration && this.delegate_.getDefaultConfiguration();
  }
  SensorStub.prototype.addConfiguration = function(configuration) {
    return this.delegate_ && this.delegate_.addConfiguration && this.delegate_.addConfiguration(configuration);
  }
  SensorStub.prototype.removeConfiguration = function(configuration) {
    return this.delegate_ && this.delegate_.removeConfiguration && this.delegate_.removeConfiguration(configuration);
  }
  SensorStub.prototype.suspend = function() {
    return this.delegate_ && this.delegate_.suspend && this.delegate_.suspend();
  }
  SensorStub.prototype.resume = function() {
    return this.delegate_ && this.delegate_.resume && this.delegate_.resume();
  }
  SensorStub.prototype.configureReadingChangeNotifications = function(enabled) {
    return this.delegate_ && this.delegate_.configureReadingChangeNotifications && this.delegate_.configureReadingChangeNotifications(enabled);
  }

  SensorStub.prototype.accept = function(message) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    case kSensor_RemoveConfiguration_Name:
      var params = reader.decodeStruct(Sensor_RemoveConfiguration_Params);
      this.removeConfiguration(params.configuration);
      return true;
    case kSensor_Suspend_Name:
      var params = reader.decodeStruct(Sensor_Suspend_Params);
      this.suspend();
      return true;
    case kSensor_Resume_Name:
      var params = reader.decodeStruct(Sensor_Resume_Params);
      this.resume();
      return true;
    case kSensor_ConfigureReadingChangeNotifications_Name:
      var params = reader.decodeStruct(Sensor_ConfigureReadingChangeNotifications_Params);
      this.configureReadingChangeNotifications(params.enabled);
      return true;
    default:
      return false;
    }
  };

  SensorStub.prototype.acceptWithResponder =
      function(message, responder) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    case kSensor_GetDefaultConfiguration_Name:
      var params = reader.decodeStruct(Sensor_GetDefaultConfiguration_Params);
      this.getDefaultConfiguration().then(function(response) {
        var responseParams =
            new Sensor_GetDefaultConfiguration_ResponseParams();
        responseParams.configuration = response.configuration;
        var builder = new codec.MessageV1Builder(
            kSensor_GetDefaultConfiguration_Name,
            codec.align(Sensor_GetDefaultConfiguration_ResponseParams.encodedSize),
            codec.kMessageIsResponse, reader.requestID);
        builder.encodeStruct(Sensor_GetDefaultConfiguration_ResponseParams,
                             responseParams);
        var message = builder.finish();
        responder.accept(message);
      });
      return true;
    case kSensor_AddConfiguration_Name:
      var params = reader.decodeStruct(Sensor_AddConfiguration_Params);
      this.addConfiguration(params.configuration).then(function(response) {
        var responseParams =
            new Sensor_AddConfiguration_ResponseParams();
        responseParams.success = response.success;
        var builder = new codec.MessageV1Builder(
            kSensor_AddConfiguration_Name,
            codec.align(Sensor_AddConfiguration_ResponseParams.encodedSize),
            codec.kMessageIsResponse, reader.requestID);
        builder.encodeStruct(Sensor_AddConfiguration_ResponseParams,
                             responseParams);
        var message = builder.finish();
        responder.accept(message);
      });
      return true;
    default:
      return false;
    }
  };

  function validateSensorRequest(messageValidator) {
    var message = messageValidator.message;
    var paramsClass = null;
    switch (message.getName()) {
      case kSensor_GetDefaultConfiguration_Name:
        if (message.expectsResponse())
          paramsClass = Sensor_GetDefaultConfiguration_Params;
      break;
      case kSensor_AddConfiguration_Name:
        if (message.expectsResponse())
          paramsClass = Sensor_AddConfiguration_Params;
      break;
      case kSensor_RemoveConfiguration_Name:
        if (!message.expectsResponse() && !message.isResponse())
          paramsClass = Sensor_RemoveConfiguration_Params;
      break;
      case kSensor_Suspend_Name:
        if (!message.expectsResponse() && !message.isResponse())
          paramsClass = Sensor_Suspend_Params;
      break;
      case kSensor_Resume_Name:
        if (!message.expectsResponse() && !message.isResponse())
          paramsClass = Sensor_Resume_Params;
      break;
      case kSensor_ConfigureReadingChangeNotifications_Name:
        if (!message.expectsResponse() && !message.isResponse())
          paramsClass = Sensor_ConfigureReadingChangeNotifications_Params;
      break;
    }
    if (paramsClass === null)
      return validator.validationError.NONE;
    return paramsClass.validate(messageValidator, messageValidator.message.getHeaderNumBytes());
  }

  function validateSensorResponse(messageValidator) {
   var message = messageValidator.message;
   var paramsClass = null;
   switch (message.getName()) {
      case kSensor_GetDefaultConfiguration_Name:
        if (message.isResponse())
          paramsClass = Sensor_GetDefaultConfiguration_ResponseParams;
        break;
      case kSensor_AddConfiguration_Name:
        if (message.isResponse())
          paramsClass = Sensor_AddConfiguration_ResponseParams;
        break;
    }
    if (paramsClass === null)
      return validator.validationError.NONE;
    return paramsClass.validate(messageValidator, messageValidator.message.getHeaderNumBytes());
  }

  var Sensor = {
    name: 'device.mojom.Sensor',
    kVersion: 0,
    ptrClass: SensorPtr,
    proxyClass: SensorProxy,
    stubClass: SensorStub,
    validateRequest: validateSensorRequest,
    validateResponse: validateSensorResponse,
  };
  SensorStub.prototype.validator = validateSensorRequest;
  SensorProxy.prototype.validator = validateSensorResponse;
  var kSensorClient_RaiseError_Name = 0;
  var kSensorClient_SensorReadingChanged_Name = 1;

  function SensorClientPtr(handleOrPtrInfo) {
    this.ptr = new bindings.InterfacePtrController(SensorClient,
                                                   handleOrPtrInfo);
  }

  function SensorClientAssociatedPtr(associatedInterfacePtrInfo) {
    this.ptr = new associatedBindings.AssociatedInterfacePtrController(
        SensorClient, associatedInterfacePtrInfo);
  }

  SensorClientAssociatedPtr.prototype =
      Object.create(SensorClientPtr.prototype);
  SensorClientAssociatedPtr.prototype.constructor =
      SensorClientAssociatedPtr;

  function SensorClientProxy(receiver) {
    this.receiver_ = receiver;
  }
  SensorClientPtr.prototype.raiseError = function() {
    return SensorClientProxy.prototype.raiseError
        .apply(this.ptr.getProxy(), arguments);
  };

  SensorClientProxy.prototype.raiseError = function() {
    var params = new SensorClient_RaiseError_Params();
    var builder = new codec.MessageV0Builder(
        kSensorClient_RaiseError_Name,
        codec.align(SensorClient_RaiseError_Params.encodedSize));
    builder.encodeStruct(SensorClient_RaiseError_Params, params);
    var message = builder.finish();
    this.receiver_.accept(message);
  };
  SensorClientPtr.prototype.sensorReadingChanged = function() {
    return SensorClientProxy.prototype.sensorReadingChanged
        .apply(this.ptr.getProxy(), arguments);
  };

  SensorClientProxy.prototype.sensorReadingChanged = function() {
    var params = new SensorClient_SensorReadingChanged_Params();
    var builder = new codec.MessageV0Builder(
        kSensorClient_SensorReadingChanged_Name,
        codec.align(SensorClient_SensorReadingChanged_Params.encodedSize));
    builder.encodeStruct(SensorClient_SensorReadingChanged_Params, params);
    var message = builder.finish();
    this.receiver_.accept(message);
  };

  function SensorClientStub(delegate) {
    this.delegate_ = delegate;
  }
  SensorClientStub.prototype.raiseError = function() {
    return this.delegate_ && this.delegate_.raiseError && this.delegate_.raiseError();
  }
  SensorClientStub.prototype.sensorReadingChanged = function() {
    return this.delegate_ && this.delegate_.sensorReadingChanged && this.delegate_.sensorReadingChanged();
  }

  SensorClientStub.prototype.accept = function(message) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    case kSensorClient_RaiseError_Name:
      var params = reader.decodeStruct(SensorClient_RaiseError_Params);
      this.raiseError();
      return true;
    case kSensorClient_SensorReadingChanged_Name:
      var params = reader.decodeStruct(SensorClient_SensorReadingChanged_Params);
      this.sensorReadingChanged();
      return true;
    default:
      return false;
    }
  };

  SensorClientStub.prototype.acceptWithResponder =
      function(message, responder) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    default:
      return false;
    }
  };

  function validateSensorClientRequest(messageValidator) {
    var message = messageValidator.message;
    var paramsClass = null;
    switch (message.getName()) {
      case kSensorClient_RaiseError_Name:
        if (!message.expectsResponse() && !message.isResponse())
          paramsClass = SensorClient_RaiseError_Params;
      break;
      case kSensorClient_SensorReadingChanged_Name:
        if (!message.expectsResponse() && !message.isResponse())
          paramsClass = SensorClient_SensorReadingChanged_Params;
      break;
    }
    if (paramsClass === null)
      return validator.validationError.NONE;
    return paramsClass.validate(messageValidator, messageValidator.message.getHeaderNumBytes());
  }

  function validateSensorClientResponse(messageValidator) {
    return validator.validationError.NONE;
  }

  var SensorClient = {
    name: 'device.mojom.SensorClient',
    kVersion: 0,
    ptrClass: SensorClientPtr,
    proxyClass: SensorClientProxy,
    stubClass: SensorClientStub,
    validateRequest: validateSensorClientRequest,
    validateResponse: null,
  };
  SensorClientStub.prototype.validator = validateSensorClientRequest;
  SensorClientProxy.prototype.validator = null;
  exports.SensorType = SensorType;
  exports.ReportingMode = ReportingMode;
  exports.SensorConfiguration = SensorConfiguration;
  exports.Sensor = Sensor;
  exports.SensorPtr = SensorPtr;
  exports.SensorAssociatedPtr = SensorAssociatedPtr;
  exports.SensorClient = SensorClient;
  exports.SensorClientPtr = SensorClientPtr;
  exports.SensorClientAssociatedPtr = SensorClientAssociatedPtr;
})();
