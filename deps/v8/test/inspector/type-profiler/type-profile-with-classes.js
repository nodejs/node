// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const source =
  `
function f(n) {
};
f(5);
function g(a, b, c) {
  return 'bye';
};
class Tree {};
class Flower extends Tree{};
var f = new Flower();
f.constructor = {};
f.constructor.name = "Not a flower.";
g({}, [], f);
g(3, 2.3, {a: 42});
`;

let {session, contextGroup, Protocol} = InspectorTest.start("Test collecting type profile data with Profiler.takeTypeProfile.");

(async function testTypeProfile(next) {
  await Protocol.Profiler.enable();
  await Protocol.Profiler.startTypeProfile();

  Protocol.Runtime.enable();
  let {result: {scriptId}} = await Protocol.Runtime.compileScript({ expression: source,
    sourceURL: arguments.callee.name, persistScript: true });

  Protocol.Runtime.runScript({ scriptId });

  let typeProfiles = await Protocol.Profiler.takeTypeProfile();
  await session.logTypeProfile(typeProfiles.result.result[0],
    source);

  Protocol.Profiler.disable();
  await Protocol.Runtime.disable();
  InspectorTest.completeTest();
})();
