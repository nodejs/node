// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function get_a() { return "aaaaaaaaaaaaaa"; }
function get_b() { return "bbbbbbbbbbbbbb"; }

function get_string() {
  return get_a() + get_b();
}

function prefix(s) {
  return s + get_string();
}

prefix("");
prefix("");
%OptimizeFunctionOnNextCall(prefix);
var s = prefix("");
assertFalse(s === "aaaaaaaaaaaaaabbbbbbbbbbbbbc");
