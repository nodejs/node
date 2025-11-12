// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupArrayReduce(length) {
  let a = new Array(length);
  for (let i=0;i<length;i++) {
    a[i] = ''+i;
  }
  return Object.freeze(a);
}

const kArraylength = 200;
const frozenArrayReduce = setupArrayReduce(kArraylength);

const reducer = (accumulator, currentValue) => accumulator + Number(currentValue);

function driverArrayReduce(n) {
  let result = 0;
  for (let i=0;i<n;i++) {
    result = frozenArrayReduce.reduce(reducer);
  }
  return result;
}

const kIterations = 1e3;

function ArrayReduce() {
  driverArrayReduce(kIterations);
}

const kIterationsWarmUp = [1e1, 1e2];

function ArrayReduceWarmUp() {
  driverArrayReduce(kIterationsWarmUp[0]);
  driverArrayReduce(kIterationsWarmUp[1]);
}

const kRun = 1e1;
createSuite('ArrayReduce', kRun, ArrayReduce, ArrayReduceWarmUp);

function driverArrayReduceRight(n) {
  let result = 0;
  for (let i=0;i<n;i++) {
    result = frozenArrayReduce.reduceRight(reducer);
  }
  return result;
}

function ArrayReduceRight() {
  driverArrayReduceRight(kIterations);
}

function ArrayReduceRightWarmUp() {
  driverArrayReduceRight(kIterationsWarmUp[0]);
  driverArrayReduceRight(kIterationsWarmUp[1]);
}

createSuite('ArrayReduceRight', kRun, ArrayReduceRight, ArrayReduceRightWarmUp);
