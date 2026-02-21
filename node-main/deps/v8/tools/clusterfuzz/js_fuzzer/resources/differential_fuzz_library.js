// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Helpers for printing in correctness fuzzing.

// Global helper functions for printing.
var __prettyPrint;
var __prettyPrintExtra;

// Track caught exceptions.
var __caught = 0;

// Track a hash of all printed values - printing is cut off after a
// certain size.
var __hash = 0;

(function() {
  const charCodeAt = String.prototype.charCodeAt;
  const dateValueOf = Date.prototype.valueOf;
  const filter = Array.prototype.filter;
  const from = Array.from;
  const funToString = Function.prototype.toString
  const isInteger = Number.isInteger;
  const join = Array.prototype.join;
  const objectKeys = Object.keys;
  const map = Array.prototype.map;
  const regExpToString = RegExp.prototype.toString;
  const substring = String.prototype.substring;
  const stringify = JSON.stringify;
  const safeValueOf = Object.prototype.valueOf;
  const safeArray = Array;
  const safeBigInt = BigInt;
  const safeBoolean = Boolean;
  const safeDate = Date;
  const safeMap = Map;
  const safeNumber = Number
  const safeRegExp = RegExp;
  const safeSet = Set;
  const safeString = String;

  const safeInt8Array = Int8Array;
  const safeUint8Array = Uint8Array;
  const safeUint8ClampedArray = Uint8ClampedArray;
  const safeInt16Array = Int16Array;
  const safeUint16Array = Uint16Array;
  const safeInt32Array = Int32Array;
  const safeUint32Array = Uint32Array;
  const safeFloat32Array = Float32Array;
  const safeFloat64Array = Float64Array;
  const safeBigInt64Array = BigInt64Array;
  const safeBigUint64Array = BigUint64Array;

  const wrapped = (inner) => inner ? `{${inner}}` : "";

  // Override prettyPrinted with a version that also recursively prints
  // objects, arrays and object properties with a depth of 4. We don't track
  // circles, but we'd cut off after a depth of 4 if there are any.
  prettyPrinted = function prettyPrinted(value, depth=4) {
    if (depth <= 0) {
      return "...";
    }
    switch (typeof value) {
      case "string":
        return stringify(value);
      case "bigint":
        return safeString(value) + "n";
      case "number":
        if (value === 0 && (1 / value) < 0) return "-0";
      case "boolean":
      case "undefined":
      case "symbol":
        return safeString(value);
      case "function":
        return prettyPrintedFunction(value, depth);
      case "object":
        if (value === null) return "null";
        if (value instanceof safeArray) return prettyPrintedArray(value, depth);
        if (value instanceof safeSet) return prettyPrintedSet(value, depth);
        if (value instanceof safeMap) return prettyPrintedMap(value, depth);
        if (value instanceof safeRegExp) return prettyPrintedRegExp(value, depth);
        if (value instanceof safeDate) return prettyPrintedDate(value, depth);

        if (value instanceof safeNumber ||
            value instanceof safeBigInt ||
            value instanceof safeString ||
            value instanceof safeBoolean) {
          return prettyWithClass(value, depth);
        }

        if (value instanceof safeInt8Array ||
            value instanceof safeUint8Array ||
            value instanceof safeUint8ClampedArray ||
            value instanceof safeInt16Array ||
            value instanceof safeUint16Array ||
            value instanceof safeInt32Array ||
            value instanceof safeUint32Array ||
            value instanceof safeFloat32Array ||
            value instanceof safeFloat64Array ||
            value instanceof safeBigInt64Array ||
            value instanceof safeBigUint64Array) {
          return prettyPrintedTypedArray(value, depth);
        }

        return prettyPrintedObject(value, depth);
    }
    return safeString(value);
  }

  function prettyPrintedObjectProperties(object, depth, forArray) {
    let keys = objectKeys(object);
    if (forArray) keys = filter.call(keys, (n) => !isInteger(safeNumber(n)));
    const prettyValues = map.call(keys, (key) => {
      return `${key}: ${prettyPrinted(object[key], depth - 1)}`;
    });
    return join.call(prettyValues, ", ");
  }

  function prettyPrintedArray(array, depth) {
    const result = map.call(array, (value, index, array) => {
      if (value === undefined && !(index in array)) return "";
      return prettyPrinted(value, depth - 1);
    });
    const props = prettyPrintedObjectProperties(array, depth, true);
    return `[${join.call(result, ", ")}]${wrapped(props)}`;
  }

  function prettyPrintedSet(set, depth) {
    const result = prettyPrintedArray(from(set), depth);
    const props = prettyPrintedObjectProperties(set, depth, false);
    return `Set${result}${wrapped(props)}`;
  }

  function prettyPrintedMap(map, depth) {
    // Array.from creates an array of arrays (i.e. of tuples). Add depth +1
    // here since we print through 2 levels of array.
    const result = prettyPrintedArray(from(map), depth + 1, false);
    const props = prettyPrintedObjectProperties(map, depth, false);
    return `Map{${result}}${wrapped(props)}`;
  }

  function prettyPrintedObject(object, depth) {
    const content = prettyPrintedObjectProperties(object, depth, false);
    return `${object.constructor?.name ?? "Object"}{${content}}`;
  }

  function prettyWithClass(object, depth) {
    const props = prettyPrintedObjectProperties(object, depth, false);
    const name = object.constructor?.name ?? "Object";
    return `${name}(${safeValueOf.call(object)})${wrapped(props)}`;
  }

  function prettyPrintedDate(object, depth) {
    const props = prettyPrintedObjectProperties(object, depth, false);
    return `Date(${dateValueOf.call(object)})${wrapped(props)}`;
  }

  function prettyPrintedTypedArray(object, depth) {
    const props = prettyPrintedObjectProperties(object, depth, true);
    const name = object.constructor?.name ?? "Object";
    return `${name}[${join.call(object, ", ")}]${wrapped(props)}`;
  }

  function prettyPrintedRegExp(object, depth) {
    const props = prettyPrintedObjectProperties(object, depth, false);
    return `${regExpToString.call(object)}${wrapped(props)}`;
  }

  function prettyPrintedFunction(fun, depth) {
    const props = prettyPrintedObjectProperties(fun, depth, false);
    return `Fun{${funToString.call(fun)}}${wrapped(props)}`;
  }

  // Helper for calculating a hash code of a string.
  function hashCode(str) {
      let hash = 0;
      if (str.length == 0) {
          return hash;
      }
      for (let i = 0; i < str.length; i++) {
          const char = charCodeAt.call(str, i);
          hash = ((hash << 5) - hash) + char;
          hash = hash & hash;
      }
      return hash;
  }

  // Upper limit for calling extra printing. When reached, hashes of
  // strings are tracked and printed instead.
  let maxExtraPrinting = 100;

  // Helper for pretty printing.
  __prettyPrint = function(value, extra=false) {
    let str = prettyPrinted(value);

    // Change __hash with the contents of the full string to
    // keep track of differences also when we don't print.
    const hash = hashCode(str);
    __hash = hashCode(hash + __hash.toString());

    if (extra && maxExtraPrinting-- <= 0) {
      return;
    }

    // Cut off long strings to prevent overloading I/O. We still track
    // the hash of the full string.
    if (str.length > 64) {
      const head = substring.call(str, 0, 54);
      const tail = substring.call(str, str.length - 9, str.length);
      str = `${head}[...]${tail}`;
    }

    print(str);
    return value;
  };

  __prettyPrintExtra = function (value) {
    __prettyPrint(value, true);
    return value;
  }
})();
