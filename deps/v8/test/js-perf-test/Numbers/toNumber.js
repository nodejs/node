// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
const A = [undefined, 12, "123"];

function NumberConstructor() {
  Number.isNaN(Number(A[0]))
  Number.isNaN(Number(A[1]))
  Number.isNaN(Number(A[2]))
}
createSuite('Constructor', 1000, NumberConstructor, ()=>{});

function NumberPlus() {
  Number.isNaN(+(A[0]))
  Number.isNaN(+(A[1]))
  Number.isNaN(+(A[2]))
}
createSuite('UnaryPlus', 1000, NumberPlus, ()=>{});

function NumberParseFloat() {
  Number.isNaN(parseFloat(A[0]))
  Number.isNaN(parseFloat(A[1]))
  Number.isNaN(parseFloat(A[2]))
}
createSuite('ParseFloat', 1000, NumberParseFloat, ()=>{});
