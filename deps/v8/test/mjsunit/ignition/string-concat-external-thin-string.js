// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-externalize-string

// Calculate string so that it isn't internalized.
var string = createExternalizableString(((a, b) => {
  return a + b;
})('foo', 'bar'));

// Now externalize it.
externalizeString(string);

// Then internalize it by using it as a keyed property name
// to turn it a thin string.
var obj = {};
obj[string];

// Perform a string concatination.
assertEquals('"' + string + '"', '"foobar"');
