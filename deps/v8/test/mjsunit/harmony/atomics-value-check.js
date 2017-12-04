// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-sharedarraybuffer
//

var sab = new SharedArrayBuffer(4);
var sta = new Int8Array(sab);
sta[0] = 5;
var workerScript =
      `onmessage=function(msg) {
         postMessage(0);
       };`;
var worker = new Worker(workerScript);

var value_obj = {
  valueOf() {worker.postMessage({sab:sab}, [sta.buffer]);
                       return 5}
}
var value = Object.create(value_obj);

assertThrows(() => {Atomics.exchange(sta, 0, value)});
assertThrows(() => {Atomics.compareExchange(sta, 0, 5, value)});
assertThrows(() => {Atomics.compareExchange(sta, 0, value, 5)});
assertThrows(() => {Atomics.add(sta, 0, value)});
assertThrows(() => {Atomics.sub(sta, 0, value)});
assertThrows(() => {Atomics.and(sta, 0, value)});
assertThrows(() => {Atomics.or(sta, 0, value)});
assertThrows(() => {Atomics.xor(sta, 0, value)});

var index_obj = {
  valueOf() {worker.postMessage({sab:sab}, [sta.buffer]);
                       return 0}
}
var index = Object.create(index_obj);

assertThrows(() => {Atomics.exchange(sta, index, 1)});
assertThrows(() => {Atomics.compareExchange(sta, index, 5, 2)});
assertThrows(() => {Atomics.add(sta, index, 3)});
assertThrows(() => {Atomics.sub(sta, index, 4)});
assertThrows(() => {Atomics.and(sta, index, 5)});
assertThrows(() => {Atomics.or(sta, index, 6)});
assertThrows(() => {Atomics.xor(sta, index, 7)});
