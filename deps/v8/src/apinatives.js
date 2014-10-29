// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This file contains infrastructure used by the API.  See
// v8natives.js for an explanation of these files are processed and
// loaded.


function CreateDate(time) {
  var date = new $Date();
  date.setTime(time);
  return date;
}


var kApiFunctionCache = new InternalArray();
var functionCache = kApiFunctionCache;


function Instantiate(data, name) {
  if (!%IsTemplate(data)) return data;
  var tag = %GetTemplateField(data, kApiTagOffset);
  switch (tag) {
    case kFunctionTag:
      return InstantiateFunction(data, name);
    case kNewObjectTag:
      var Constructor = %GetTemplateField(data, kApiConstructorOffset);
      // Note: Do not directly use a function template as a condition, our
      // internal ToBoolean doesn't handle that!
      var result;
      if (typeof Constructor === 'undefined') {
        result = {};
        ConfigureTemplateInstance(result, data);
      } else {
        // ConfigureTemplateInstance is implicitly called before calling the API
        // constructor in HandleApiCall.
        result = new (Instantiate(Constructor))();
        result = %ToFastProperties(result);
      }
      return result;
    default:
      throw 'Unknown API tag <' + tag + '>';
  }
}


function InstantiateFunction(data, name) {
  // We need a reference to kApiFunctionCache in the stack frame
  // if we need to bail out from a stack overflow.
  var cache = kApiFunctionCache;
  var serialNumber = %GetTemplateField(data, kApiSerialNumberOffset);
  var isFunctionCached =
   (serialNumber in cache) && (cache[serialNumber] != kUninitialized);
  if (!isFunctionCached) {
    try {
      var flags = %GetTemplateField(data, kApiFlagOffset);
      var prototype;
      if (!(flags & (1 << kRemovePrototypeBit))) {
        var template = %GetTemplateField(data, kApiPrototypeTemplateOffset);
        prototype = typeof template === 'undefined'
            ?  {} : Instantiate(template);

        var parent = %GetTemplateField(data, kApiParentTemplateOffset);
        // Note: Do not directly use a function template as a condition, our
        // internal ToBoolean doesn't handle that!
        if (typeof parent !== 'undefined') {
          var parent_fun = Instantiate(parent);
          %InternalSetPrototype(prototype, parent_fun.prototype);
        }
      }
      var fun = %CreateApiFunction(data, prototype);
      if (IS_STRING(name)) %FunctionSetName(fun, name);
      var doNotCache = flags & (1 << kDoNotCacheBit);
      if (!doNotCache) cache[serialNumber] = fun;
      ConfigureTemplateInstance(fun, data);
      if (doNotCache) return fun;
    } catch (e) {
      cache[serialNumber] = kUninitialized;
      throw e;
    }
  }
  return cache[serialNumber];
}


function ConfigureTemplateInstance(obj, data) {
  var properties = %GetTemplateField(data, kApiPropertyListOffset);
  if (!properties) return;
  // Disable access checks while instantiating the object.
  var requires_access_checks = %DisableAccessChecks(obj);
  try {
    for (var i = 1; i < properties[0];) {
      var length = properties[i];
      if (length == 3) {
        var name = properties[i + 1];
        var prop_data = properties[i + 2];
        var attributes = properties[i + 3];
        var value = Instantiate(prop_data, name);
        %AddPropertyForTemplate(obj, name, value, attributes);
      } else if (length == 4 || length == 5) {
        // TODO(verwaest): The 5th value used to be access_control. Remove once
        // the bindings are updated.
        var name = properties[i + 1];
        var getter = properties[i + 2];
        var setter = properties[i + 3];
        var attribute = properties[i + 4];
        %DefineApiAccessorProperty(obj, name, getter, setter, attribute);
      } else {
        throw "Bad properties array";
      }
      i += length + 1;
    }
  } finally {
    if (requires_access_checks) %EnableAccessChecks(obj);
  }
}
