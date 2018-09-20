// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --type-profile

const source =
  `
function g(a, b, c) {
  return 'bye';
};
class Tree {};
class Flower extends Tree{};
var f = new Flower();
// We store the type when a variable is used. If a toStringTag is
// changes the type, we want to collect that changed feedback.
// This tests ensures that we collect that information rather than
// for example infer the types from the internal map, which wouldn't
// know about a toStringTag.
f[Symbol.toStringTag] = 'Dog';
g({}, [], f);
g(3, 2.3, {a: 42});
`;

let {session, contextGroup, Protocol} = InspectorTest.start("Test collecting " +
  "type profile data with Profiler.takeTypeProfile.");

(async function testTypeProfile() {
  await Protocol.Profiler.enable();
  await Protocol.Profiler.startTypeProfile();

  Protocol.Runtime.enable();
  let {result: {scriptId}} = await Protocol.Runtime.compileScript({ expression: source,
    sourceURL: arguments.callee.name, persistScript: true });
  Protocol.Runtime.runScript({ scriptId });
  await Protocol.Profiler.startTypeProfile();

  let typeProfiles = await Protocol.Profiler.takeTypeProfile();
  await session.logTypeProfile(typeProfiles.result.result[0],
    source);

  Protocol.Profiler.stopTypeProfile();
  Protocol.Profiler.disable();
  await Protocol.Runtime.disable();
  InspectorTest.completeTest();
})();
