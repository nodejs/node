// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
function DateToLocaleDateString() {
  let d = new Date();
  d.toLocaleDateString()
}
createSuite('toLocaleDateString', 100000, DateToLocaleDateString, ()=>{});

function DateToLocaleString() {
  let d = new Date();
  d.toLocaleString()
}
createSuite('toLocaleString', 100000, DateToLocaleString, ()=>{});

function DateToLocaleTimeString() {
  let d = new Date();
  d.toLocaleTimeString()
}
createSuite('toLocaleTimeString', 100000, DateToLocaleTimeString, ()=>{});
