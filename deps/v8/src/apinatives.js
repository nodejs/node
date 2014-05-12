// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
      var result = typeof Constructor === 'undefined' ?
          {} : new (Instantiate(Constructor))();
      ConfigureTemplateInstance(result, data);
      result = %ToFastProperties(result);
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
      var has_proto = !(flags & (1 << kRemovePrototypeBit));
      var prototype;
      if (has_proto) {
        var template = %GetTemplateField(data, kApiPrototypeTemplateOffset);
        prototype = typeof template === 'undefined'
            ?  {} : Instantiate(template);

        var parent = %GetTemplateField(data, kApiParentTemplateOffset);
        // Note: Do not directly use a function template as a condition, our
        // internal ToBoolean doesn't handle that!
        if (typeof parent !== 'undefined') {
          var parent_fun = Instantiate(parent);
          %SetPrototype(prototype, parent_fun.prototype);
        }
      }
      var fun = %CreateApiFunction(data, prototype);
      if (name) %FunctionSetName(fun, name);
      var doNotCache = flags & (1 << kDoNotCacheBit);
      if (!doNotCache) cache[serialNumber] = fun;
      if (has_proto && flags & (1 << kReadOnlyPrototypeBit)) {
        %FunctionSetReadOnlyPrototype(fun);
      }
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
        %SetProperty(obj, name, value, attributes);
      } else if (length == 5) {
        var name = properties[i + 1];
        var getter = properties[i + 2];
        var setter = properties[i + 3];
        var attribute = properties[i + 4];
        var access_control = properties[i + 5];
        %SetAccessorProperty(
            obj, name, getter, setter, attribute, access_control);
      } else {
        throw "Bad properties array";
      }
      i += length + 1;
    }
  } finally {
    if (requires_access_checks) %EnableAccessChecks(obj);
  }
}
