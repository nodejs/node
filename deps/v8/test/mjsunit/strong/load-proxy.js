// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-proxies --strong-mode

// Forwarding proxies adapted from proposal definition
function handlerMaker1(obj) {
  return {
    getPropertyDescriptor: function(name) {
      var desc;
      var searchObj = obj;
      while (desc === undefined && searchObj != null) {
        desc = Object.getOwnPropertyDescriptor(searchObj, name);
        searchObj = searchObj.__proto__;
      }
      // a trapping proxy's properties must always be configurable
      if (desc !== undefined) { desc.configurable = true; }
      return desc;
    },
    fix: function() {
      if (Object.isFrozen(obj)) {
        var result = {};
        Object.getOwnPropertyNames(obj).forEach(function(name) {
          result[name] = Object.getOwnPropertyDescriptor(obj, name);
        });
        return result;
      }
      // As long as obj is not frozen, the proxy won't allow itself to be fixed
      return undefined; // will cause a TypeError to be thrown
    }
  };
}
function handlerMaker2(obj) {
  return {
    get: function(receiver, name) {
      return obj[name];
    },
    fix: function() {
      if (Object.isFrozen(obj)) {
        var result = {};
        Object.getOwnPropertyNames(obj).forEach(function(name) {
          result[name] = Object.getOwnPropertyDescriptor(obj, name);
        });
        return result;
      }
      // As long as obj is not frozen, the proxy won't allow itself to be fixed
      return undefined; // will cause a TypeError to be thrown
    }
  };
}
var baseObj = {};
var proxy1 = Proxy.create(handlerMaker1(baseObj));
var proxy2 = Proxy.create(handlerMaker2(baseObj));
var childObj1 = { __proto__: proxy1 };
var childObj2 = { __proto__: proxy2 };
var childObjAccessor1 = { set foo(_){}, set "1"(_){}, __proto__: proxy1 };
var childObjAccessor2 = { set foo(_){}, set "1"(_){}, __proto__: proxy2 };

(function() {
  "use strong";
  // TODO(conradw): These asserts are sanity checking V8's proxy implementation.
  // Strong mode semantics for ES6 proxies still need to be explored.
  assertDoesNotThrow(function(){proxy1.foo});
  assertDoesNotThrow(function(){proxy1[1]});
  assertDoesNotThrow(function(){proxy2.foo});
  assertDoesNotThrow(function(){proxy2[1]});
  assertDoesNotThrow(function(){childObj1.foo});
  assertDoesNotThrow(function(){childObj1[1]});
  assertDoesNotThrow(function(){childObj2.foo});
  assertDoesNotThrow(function(){childObj2[1]});
  assertThrows(function(){baseObj.foo}, TypeError);
  assertThrows(function(){baseObj[1]}, TypeError);
  assertThrows(function(){childObjAccessor1.foo}, TypeError);
  assertThrows(function(){childObjAccessor1[1]}, TypeError);
  assertThrows(function(){childObjAccessor2.foo}, TypeError);
  assertThrows(function(){childObjAccessor2[1]}, TypeError);

  // Once the proxy is no longer trapping, property access should have strong
  // semantics.
  Object.freeze(baseObj);

  Object.freeze(proxy1);
  assertThrows(function(){proxy1.foo}, TypeError);
  assertThrows(function(){proxy1[1]}, TypeError);
  assertThrows(function(){childObj1.foo}, TypeError);
  assertThrows(function(){childObj1[1]}, TypeError);
  assertThrows(function(){childObjAccessor1.foo}, TypeError);
  assertThrows(function(){childObjAccessor1[1]}, TypeError);

  Object.freeze(proxy2);
  assertThrows(function(){proxy2.foo}, TypeError);
  assertThrows(function(){proxy2[1]}, TypeError);
  assertThrows(function(){childObj2.foo}, TypeError);
  assertThrows(function(){childObj2[1]}, TypeError);
  assertThrows(function(){childObjAccessor2.foo}, TypeError);
  assertThrows(function(){childObjAccessor2[1]}, TypeError);
})();
