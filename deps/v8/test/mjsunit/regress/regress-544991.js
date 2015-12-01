// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var typedArray = new Int8Array(1);
var saved;
var called;
typedArray.constructor = function(x) { called = true; saved = x };
typedArray.constructor.prototype = Int8Array.prototype;
typedArray.map(function(){});

// To meet the spec, constructor shouldn't be called directly, but
// if it is called for now, the argument should be an Array
assertTrue(called); // Will fail later; when so, delete this test
assertEquals("Array", saved.constructor.name);
