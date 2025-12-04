// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('Try-Catch', [1000], [
  new Benchmark('OnSuccess', false, false, 0,
                OnSuccess, OnSuccessSetup,
                OnSuccessTearDown),
  new Benchmark('OnException', false, false, 0,
                OnException, OnExceptionSetup,
                OnExceptionTearDown),
  new Benchmark('OnSuccessFinallyOnly', false, false, 0,
                OnSuccessFinallyOnly, OnSuccessFinallyOnlySetup,
                OnSuccessFinallyOnlyTearDown),
  new Benchmark('WithFinallyOnException', false, false, 0,
                WithFinallyOnException, WithFinallyOnExceptionSetup,
                WithFinallyOnExceptionTearDown)
]);

var a;
var b;
var c;

// ----------------------------------------------------------------------------

function OnSuccessSetup() {
  a = 4;
  b = 6;
  c = 0;
}

function OnSuccess() {
  try {
    c = a + b;
  }
  catch (e) {
    c++;
  }
}

function OnSuccessTearDown() {
  return c === 10;
}

// ----------------------------------------------------------------------------

function OnExceptionSetup() {
  a = 4;
  b = 6;
  c = 0;
}

function OnException() {
  try {
    throw 'Test exception';
  }
  catch (e) {
    c = a + b;
  }
}

function OnExceptionTearDown() {
  return c === 10;
}

// ----------------------------------------------------------------------------

function OnSuccessFinallyOnlySetup() {
  a = 4;
  b = 6;
  c = 0;
}

function OnSuccessFinallyOnly() {
  try {
    c = a + b;
  }
  finally {
    c++;
  }
}

function OnSuccessFinallyOnlyTearDown() {
  return c === 11;
}

// ----------------------------------------------------------------------------

function WithFinallyOnExceptionSetup() {
  a = 4;
  b = 6;
  c = 0;
}

function WithFinallyOnException() {
  try {
    throw 'Test exception';
  }
  catch (e) {
    c = a + b;
  }
  finally {
    c++;
  }
}

function WithFinallyOnExceptionTearDown() {
  return c === 11;
}
// ----------------------------------------------------------------------------
