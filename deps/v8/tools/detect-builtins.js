// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global) {

  var GetProperties = function(this_name, object) {
    var result = {};
    try {
      var names = Object.getOwnPropertyNames(object);
    } catch(e) {
      return;
    }
    for (var i = 0; i < names.length; ++i) {
      var name = names[i];
      if (typeof object === "function") {
        if (name === "length" ||
            name === "name" ||
            name === "arguments" ||
            name === "caller" ||
            name === "prototype") {
          continue;
        }
      }
      // Avoid endless recursion.
      if (this_name === "prototype" && name === "constructor") continue;
      // Avoid needless duplication.
      if (this_name === "__PROTO__" && name === "constructor") continue;
      // Could get this from the parent, but having it locally is easier.
      var property = { "name": name };
      try {
        var value = object[name];
      } catch(e) {
        property.type = "getter";
        result[name] = property;
        continue;
      }
      var type = typeof value;
      property.type = type;
      if (type === "function") {
        property.length = value.length;
        property.prototype = GetProperties("prototype", value.prototype);
      }
      if (type === "string" || type === "number") {
        property.value = value;
      } else {
        property.properties = GetProperties(name, value);
      }
      result[name] = property;
    }
    // Print the __proto__ if it's not the default Object prototype.
    if (typeof object === "object" && object.__proto__ !== null &&
        !object.__proto__.hasOwnProperty("__proto__")) {
      result.__PROTO__ = GetProperties("__PROTO__", object.__proto__);
    }
    return result;
  };

  var g = GetProperties("", global, "");
  print(JSON.stringify(g, undefined, 2));

})(this);  // Must wrap in anonymous closure or it'll detect itself as builtin.
