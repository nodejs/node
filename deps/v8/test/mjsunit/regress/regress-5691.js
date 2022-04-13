// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

var log = "";
var result;
Promise.resolve()
  .then(() => log += "|turn1")
  .then(() => log += "|turn2")
  .then(() => log += "|turn3")
  .then(() => log += "|turn4")
  .then(() => result = "|start|turn1|fast-resolve|turn2|turn3|slow-resolve|turn4\n"+log)
  .catch(e => print("ERROR", e));

Promise.resolve(Promise.resolve()).then(() => log += "|fast-resolve");
(class extends Promise {}).resolve(Promise.resolve()).then(() => log += "|slow-resolve");

log += "|start";
%PerformMicrotaskCheckpoint();
assertEquals("|start|turn1|fast-resolve|turn2|turn3|slow-resolve|turn4\n\
|start|turn1|fast-resolve|turn2|turn3|slow-resolve|turn4", result);
