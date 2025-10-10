// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly modified variants from http://code.fitness/post/2016/01/javascript-enumerate-methods.html.
function __isPropertyOfType(obj, name, type) {
  let desc;
  try {
    desc = Object.getOwnPropertyDescriptor(obj, name);
  } catch(e) {
    return false;
  }

  if (!desc)
    return false;

  return typeof type === 'undefined' || typeof desc.value === type;
}

function __getProperties(obj, type) {
  if (typeof obj === "undefined" || obj === null)
    return [];

  let properties = [];
  for (let name of Object.getOwnPropertyNames(obj)) {
    if (__isPropertyOfType(obj, name, type))
      properties.push(name);
  }

  let proto = Object.getPrototypeOf(obj);
  while (proto && proto != Object.prototype) {
    Object.getOwnPropertyNames(proto)
      .forEach (name => {
        if (name !== 'constructor') {
          if (__isPropertyOfType(proto, name, type))
            properties.push(name);
        }
      });
    proto = Object.getPrototypeOf(proto);
  }
  return properties;
}

function* __getObjects(root = this, level = 0) {
    if (level > 4)
      return;

    let obj_names = __getProperties(root, 'object');
    for (let obj_name of obj_names) {
      let obj = root[obj_name];
      if (obj === root)
        continue;

      yield obj;
      yield* __getObjects(obj, level + 1);
    }
}

var __getRandomObject;
{
  let count = 0;
  __getRandomObject = function(seed) {
    if (count++ > 50) return this;
    let objects = [];
    for (let obj of __getObjects()) {
      objects.push(obj);
    }

    return objects[seed % objects.length];
  };
}

var __getRandomProperty;
{
  let count = 0;
  __getRandomProperty = function(obj, seed) {
    if (count++ > 50) return undefined;
    let properties = __getProperties(obj);
    if (!properties.length)
      return undefined;

    return properties[seed % properties.length];
  };
}

var __callRandomFunction;
{
  let count = 0;
  __callRandomFunction = function(obj, seed, ...args)
  {
    if (count++ > 25) return;
    let functions = __getProperties(obj, 'function');
    if (!functions.length)
      return;

    let random_function = functions[seed % functions.length];
    try {
      obj[random_function](...args);
    } catch(e) { }
  };
}

function runNearStackLimit(f) {
  function t() {
    try {
      return t();
    } catch (e) {
      return f();
    }
  };
  try {
    return t();
  } catch (e) {}
}

// Limit number of times we cause GCs in tests to reduce hangs
// when called within larger loops.
let __callGC;
(function() {
  let countGC = 0;
  __callGC = function(major) {
    const type = {type: major ? 'major' : 'minor'};
    if (countGC++ < 20) {
      gc(type);
    }
  };
})();

// Limit number of times we call SetAllocationTimeout as above.
let __callSetAllocationTimeout;
(function() {
  let countCalls = 0;
  __callSetAllocationTimeout = function(timeout, inline) {
    if (countCalls++ < 20) {
      %SetAllocationTimeout(-1, timeout, inline);
    }
  };
})();

let __dummy;
(function() {
  const handler = {
    get: function(x, prop) {
      if (prop == Symbol.toPrimitive) {
        return function() { return undefined; };
      }
      return __dummy;
    },
  };
  __dummy = new Proxy(function() { return __dummy; }, handler);
  Object.freeze(__dummy);
})();

function __wrapTC(f, permissive=true) {
  try {
    return f();
  } catch (e) {
    if (permissive) {
      return __dummy;
    }
  }
}

// Neuter common test functions.
try { this.fail = nop; } catch(e) { }
try { this.failWithMessage = nop; } catch(e) { }
try { this.triggerAssertFalse = nop; } catch(e) { }
try { this.quit = nop; } catch(e) { }
