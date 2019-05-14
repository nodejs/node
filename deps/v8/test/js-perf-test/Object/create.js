// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ----------------------------------------------------------------------------

function shallowClone(object) {
  return Object.create(object.__proto__,
      Object.getOwnPropertyDescriptors(object));
}

function makeSlowCopy(object) {
  object = shallowClone(object);
  object.__foo__ = 1;
  delete object.__foo__;
  return object;
}

function convertToPropertyDescriptors(dict) {
  for (var key in dict) {
    var propertiesObject = dict[key];
    dict[key] = Object.getOwnPropertyDescriptors(propertiesObject);
  }
  return dict;
}

var properties_5 = { a:1, b:2, c:3, d:4, e:5 };
var TEST_PROPERTIES = convertToPropertyDescriptors({
  empty: {},
  array_5: [1, 2, 3, 4, 5],
  properties_5: properties_5,
  properties_10: { a:1, b:2, c:3, d:4, e:5, f:6, g:7, h:8, i:9, j:10 },
  properties_dict: makeSlowCopy(properties_5)
});

var TEST_PROTOTYPES = {
  null: null,
  empty: {},
  'Object.prototype': Object.prototype,
  'Array.prototype': Array.prototype
};

// ----------------------------------------------------------------------------

var testFunction = () => {
  return Object.create(prototype, properties);
}

function createTestFunction(prototype, properties) {
  // Force a new function for each test-object to avoid side-effects due to ICs.
  var random_comment = "\n// random comment" + Math.random() + "\n";
  return eval(random_comment + testFunction.toString());
}

// ----------------------------------------------------------------------------

var benchmarks = []

for (var proto_name in TEST_PROTOTYPES) {
  var prototype = TEST_PROTOTYPES[proto_name];
  for (var prop_name in TEST_PROPERTIES) {
    var properties = TEST_PROPERTIES[prop_name];
    var name = 'Create proto:' + proto_name + " properties:" + prop_name;
    benchmarks.push(
        new Benchmark(name, false, false, 0,
          createTestFunction(prototype, properties)));
  }
}

new BenchmarkSuite('Create', [1000], benchmarks);
