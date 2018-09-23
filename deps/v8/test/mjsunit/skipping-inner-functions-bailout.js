// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --preparser-scope-analysis

// Tests for cases where PreParser must bail out of creating data for skipping
// inner functions, since it cannot replicate the Scope structure created by
// Parser.

function TestBailoutBecauseOfSloppyEvalInArrowParams() {
  let bailout = (a = function() {}, b = eval('')) => 0
  bailout();

  function not_skippable() {}
}
TestBailoutBecauseOfSloppyEvalInArrowParams();

function TestBailoutBecauseOfSloppyEvalInArrowParams2() {
  let bailout = (a = function() {}, b = eval('')) => {}
  bailout();

  function not_skippable() {}
}
TestBailoutBecauseOfSloppyEvalInArrowParams2();

function TestBailoutBecauseOfSloppyEvalInParams() {
  function bailout(a = function() {}, b = eval('')) {
    function not_skippable() {}
  }
  bailout();

  function not_skippable_either() {}
}
TestBailoutBecauseOfSloppyEvalInParams();

 // Test bailing out from 2 places.
function TestMultiBailout1() {
  function bailout(a = function() {}, b = eval('')) {
    function not_skippable() {}
  }
  bailout();

  function bailout_too(a = function() {}, b = eval('')) {
    function not_skippable_either() {}
  }
  bailout_too();
}
TestMultiBailout1();

function TestMultiBailout2() {
  function f(a = function() {}, b = eval('')) {
    function not_skippable() {}
  }
  f();

  function not_skippable_either() {
    function bailout_too(a = function() {}, b = eval('')) {
      function inner_not_skippable() {}
    }
    bailout_too();
  }
  not_skippable_either();
}
TestMultiBailout2();

function TestMultiBailout3() {
  function bailout(a = function() {}, b = eval('')) {
    function bailout_too(a = function() {}, b = eval('')) {
      function not_skippable() {}
    }
    bailout_too();
  }
  bailout();

  function not_skippable_either() {}
}
TestMultiBailout3();

// Regression test for
// https://bugs.chromium.org/p/chromium/issues/detail?id=761980. The conditions
// triggering a bailout occur in a context where we're not generating data
// anyway (inside an arrow function). (This needs to be at top level.)
x => { (y=eval()) => {} }
