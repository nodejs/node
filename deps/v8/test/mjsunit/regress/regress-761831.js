// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function OrigReproCase() {
  assertThrows('var f = ([x=[a=undefined]=[]]) => {}; f();', TypeError);
}
OrigReproCase();

function SimpleReproCase() {
  assertThrows('var f = ([x=[]=[]]) => {}; f()', TypeError);
}
SimpleReproCase();
