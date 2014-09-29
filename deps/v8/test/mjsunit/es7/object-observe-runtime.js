// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// These tests are meant to ensure that that the Object.observe runtime
// functions are hardened.

var obj = {};
%SetIsObserved(obj);
assertThrows(function() {
  %SetIsObserved(obj);
});

assertThrows(function() {
  %SetIsObserved(this);
});
