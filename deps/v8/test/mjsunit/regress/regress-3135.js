// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Properties are serialized once.
assertEquals('{"x":1}', JSON.stringify({ x : 1 }, ["x", 1, "x", 1]));
assertEquals('{"1":1}', JSON.stringify({ 1 : 1 }, ["x", 1, "x", 1]));
assertEquals('{"1":1}', JSON.stringify({ 1 : 1 }, ["1", 1, "1", 1]));
assertEquals('{"1":1}', JSON.stringify({ 1 : 1 }, [1, "1", 1, "1"]));

// Properties are visited at most once.
var fired = 0;
var getter_obj = { get x() { fired++; return 2; } };
assertEquals('{"x":2}', JSON.stringify(getter_obj, ["x", "y", "x"]));
assertEquals(1, fired);

// Order of the replacer array is followed.
assertEquals('{"y":4,"x":3}', JSON.stringify({ x : 3, y : 4}, ["y", "x"]));
assertEquals('{"y":4,"1":2,"x":3}',
             JSON.stringify({ x : 3, y : 4, 1 : 2 }, ["y", 1, "x"]));

// With a replacer array the value of the property is retrieved using [[Get]]
// ignoring own and enumerability.
var a = { x : 8 };
assertEquals('{"__proto__":{"__proto__":null},"x":8}',
             JSON.stringify(a, ["__proto__", "x", "__proto__"]));
a.__proto__ = { x : 7 };
assertEquals('{"__proto__":{"__proto__":{"__proto__":null},"x":7},"x":8}',
             JSON.stringify(a, ["__proto__", "x"]));
var b = { __proto__: { x: 9 } };
assertEquals('{}', JSON.stringify(b));
assertEquals('{"x":9}', JSON.stringify(b, ["x"]));
var c = {x: 10};
Object.defineProperty(c, 'x', { enumerable: false });
assertEquals('{}', JSON.stringify(c));
assertEquals('{"x":10}', JSON.stringify(c, ["x"]));

// Arrays are not affected by the replacer array.
assertEquals("[9,8,7]", JSON.stringify([9, 8, 7], [1, 1]));
var mixed_arr = [11,12,13];
mixed_arr.x = 10;
assertEquals('[11,12,13]', JSON.stringify(mixed_arr, [1, 0, 1]));

// Array elements of objects are affected.
var mixed_obj = { x : 3 };
mixed_obj[0] = 6;
mixed_obj[1] = 5;
assertEquals('{"1":5,"0":6}', JSON.stringify(mixed_obj, [1, 0, 1]));

// Nested object.
assertEquals('{"z":{"x":3},"x":1}',
             JSON.stringify({ x: 1, y:2, z: {x:3, b:4}}, ["z","x"]));

// Objects in the replacer array are ignored.
assertEquals('{}',
             JSON.stringify({ x : 1, "1": 1 }, [{}]));
assertEquals('{}',
             JSON.stringify({ x : 1, "1": 1 }, [true, undefined, null]));
assertEquals('{}',
             JSON.stringify({ x : 1, "1": 1 },
                            [{ toString: function() { return "x";} }]));
assertEquals('{}',
             JSON.stringify({ x : 1, "1": 1 },
                            [{ valueOf: function() { return 1;} }]));

// Make sure that property names that clash with the names of Object.prototype
// still works.
assertEquals('{"toString":42}', JSON.stringify({ toString: 42 }, ["toString"]));

// Number wrappers and String wrappers should be unwrapped.
assertEquals('{"1":1,"s":"s"}',
             JSON.stringify({ 1: 1, s: "s" },
                            [new Number(1), new String("s")]));
