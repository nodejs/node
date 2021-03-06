// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('base.js');

function CreateBenchmarks(constructors, withSep) {
  return constructors.map(({ ctor, name }) =>
    new Benchmark(`Join${name}`, false, false, 0, CreateJoinFn(withSep),
                  CreateSetupFn(ctor), TearDown)
  );
}

const kInitialArray = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
let result;
let arrayToJoin = [];

function CreateSetupFn(constructor) {
  return () => {
    if (constructor == BigUint64Array || constructor == BigInt64Array) {
      arrayToJoin = constructor.from(kInitialArray, x => BigInt(Math.floor(x)));
    } else {
      arrayToJoin = new constructor(kInitialArray);
    }
  }
}

function CreateJoinFn(withSep) {
  if (withSep) {
    return () => result = arrayToJoin.join(',');
  } else {
    return () => result = arrayToJoin.join('');
  }
}

function TearDown() {
  arrayToJoin = void 0;
}
