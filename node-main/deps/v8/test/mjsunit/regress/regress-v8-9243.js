// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// The special IterResultObject map that builtins use should be the same
// as the one produced by the `{value, done}` object literal.
const user = {value:undefined, done:true};

// Array iterator.
const arrayResult = (new Array())[Symbol.iterator]().next();
assertTrue(%HaveSameMap(user, arrayResult));

// Map iterator.
const mapResult = (new Map())[Symbol.iterator]().next();
assertTrue(%HaveSameMap(user, mapResult));

// Set iterator.
const setResult = (new Set())[Symbol.iterator]().next();
assertTrue(%HaveSameMap(user, setResult));

// Generator.
function* generator() {}
const generatorResult = generator().next();
assertTrue(%HaveSameMap(user, setResult));
