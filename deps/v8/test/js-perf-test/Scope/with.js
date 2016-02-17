// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite( 'With', [1000], [
  new Benchmark('AccessOnSameLevel', false, false, 0,
                AccessOnSameLevel, AccessOnSameLevelSetup,
                AccessOnSameLevelTearDown),
  new Benchmark('SetOnSameLevel', false, false, 0,
                SetOnSameLevel, SetOnSameLevelSetup,
                SetOnSameLevelTearDown),
  new Benchmark('AccessOverPrototypeChain', false, false, 0,
                AccessOverPrototypeChainSetup, AccessOverPrototypeChainSetup,
                AccessOverPrototypeChainTearDown),
  new Benchmark('CompetingScope', false, false, 0,
                CompetingScope, CompetingScopeSetup, CompetingScopeTearDown)
]);

var objectUnderTest;
var objectUnderTestExtended;
var resultStore;
var VALUE_OF_PROPERTY = 'Simply a string';
var SOME_OTHER_VALUE = 'Another value';

// ----------------------------------------------------------------------------

function AccessOnSameLevelSetup() {
  objectUnderTest = {first: VALUE_OF_PROPERTY};
}

function AccessOnSameLevel() {
  with (objectUnderTest) {
    resultStore = first;
  }
}

function AccessOnSameLevelTearDown() {
  return objectUnderTest.first === resultStore;
}

// ----------------------------------------------------------------------------

function AccessOverPrototypeChainSetup() {
  objectUnderTest = {first: VALUE_OF_PROPERTY};
  objectUnderTestExtended = Object.create(objectUnderTest);
  objectUnderTestExtended.second = 'Another string';
}

function AccessOverPrototypeChain() {
  with (objectUnderTestExtended) {
    resultStore = first;
  }
}

function AccessOverPrototypeChainTearDown() {
  return objectUnderTest.first === resultStore;
}

// ----------------------------------------------------------------------------

function CompetingScopeSetup() {
  objectUnderTest = {first: VALUE_OF_PROPERTY};
}

function CompetingScope() {
  var first = 'Not correct';
  with (objectUnderTest) {
    resultStore = first;
  }
}

function CompetingScopeTearDown() {
  return objectUnderTest.first === resultStore;
}

// ----------------------------------------------------------------------------

function SetOnSameLevelSetup() {
  objectUnderTest = {first: VALUE_OF_PROPERTY};
}

function SetOnSameLevel() {
  with (objectUnderTest) {
    first = SOME_OTHER_VALUE;
  }
}

function SetOnSameLevelTearDown() {
  return objectUnderTest.first === SOME_OTHER_VALUE;
}
