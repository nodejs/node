// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('Entries', [1000], [
  new Benchmark('Basic', false, false, 0, Basic, BasicSetup, BasicTearDown)
]);

var object;
var expected;
var result;
var symbol1;

function Basic() {
  result = Object.entries(object);
}


function BasicSetup() {
  result = undefined;
  symbol1 = Symbol('test');
  object = { a: 10 };
  object[26.0] = 'third';
  object.b = 72;
  object[symbol1] = 'TEST';
  Object.defineProperty(object, 'not-enumerable', {
      enumerable: false, value: 'nope', writable: true, configurable: true });
}


function BasicTearDown() {
  result = result.map(entry => `[${[String(entry[0]), String(entry[1])]}]`);
  return result.length === 3 &&
         result.join(', ') === '[a, 10], [26.0, third], [b, 72]';
}

// ----------------------------------------------------------------------------

new BenchmarkSuite('EntriesMegamorphic', [1000], [
  new Benchmark('BasicMegamorphic', false, false, 0, BasicMegamorphic,
                BasicMegamorphicSetup, BasicMegamorphicTearDown)
]);

function BasicMegamorphic() {
  for (var i = 0; i < object.length; ++i) {
    result[i] = Object.entries(object[i]);
  }
}


function BasicMegamorphicSetup() {
  // Create 1k objects with different maps.
  object = [];
  expected = [];
  result = [];
  for (var i=0; i<1000; i++) {
    var obj = {};
    var exp = [];
    for (var j=0; j<10; j++) {
      obj['key-'+i+'-'+j] = 'property-'+i+'-'+j;
      exp[j] = ['key-'+i+'-'+j, 'property-'+i+'-'+j];
    }
    object[i] = obj;
    expected[i] = exp;
  }
}


function BasicMegamorphicTearDown() {
  if (JSON.stringify(expected) !== JSON.stringify(result)) {
    throw new Error("FAILURE");
  }
  object = result = expected = undefined;
  return true;
}
