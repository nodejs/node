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

// __proto__ is ignored and doesn't break anything.
var a = { x : 8 };
a.__proto__ = { x : 7 };
assertEquals('{"x":8}', JSON.stringify(a, ["__proto__", "x", "__proto__"]));

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
