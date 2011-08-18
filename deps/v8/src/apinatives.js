// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file contains infrastructure used by the API.  See
// v8natives.js for an explanation of these files are processed and
// loaded.


function CreateDate(time) {
  var date = new $Date();
  date.setTime(time);
  return date;
}


const kApiFunctionCache = {};
const functionCache = kApiFunctionCache;


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
      cache[serialNumber] = null;
      var fun = %CreateApiFunction(data);
      if (name) %FunctionSetName(fun, name);
      cache[serialNumber] = fun;
      var prototype = %GetTemplateField(data, kApiPrototypeTemplateOffset);
      var flags = %GetTemplateField(data, kApiFlagOffset);
      // Note: Do not directly use an object template as a condition, our
      // internal ToBoolean doesn't handle that!
      fun.prototype = typeof prototype === 'undefined' ?
          {} : Instantiate(prototype);
      if (flags & (1 << kReadOnlyPrototypeBit)) {
        %FunctionSetReadOnlyPrototype(fun);
      }
      %SetProperty(fun.prototype, "constructor", fun, DONT_ENUM);
      var parent = %GetTemplateField(data, kApiParentTemplateOffset);
      // Note: Do not directly use a function template as a condition, our
      // internal ToBoolean doesn't handle that!
      if (!(typeof parent === 'undefined')) {
        var parent_fun = Instantiate(parent);
        fun.prototype.__proto__ = parent_fun.prototype;
      }
      ConfigureTemplateInstance(fun, data);
    } catch (e) {
      cache[serialNumber] = kUninitialized;
      throw e;
    }
  }
  return cache[serialNumber];
}


function ConfigureTemplateInstance(obj, data) {
  var properties = %GetTemplateField(data, kApiPropertyListOffset);
  if (properties) {
    // Disable access checks while instantiating the object.
    var requires_access_checks = %DisableAccessChecks(obj);
    try {
      for (var i = 0; i < properties[0]; i += 3) {
        var name = properties[i + 1];
        var prop_data = properties[i + 2];
        var attributes = properties[i + 3];
        var value = Instantiate(prop_data, name);
        %SetProperty(obj, name, value, attributes);
      }
    } finally {
      if (requires_access_checks) %EnableAccessChecks(obj);
    }
  }
}
