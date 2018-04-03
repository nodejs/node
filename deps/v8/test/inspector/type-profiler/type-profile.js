// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --type-profile

const source =
  `
function f(a, b, c) {
  return 'bye';
};
f({}, [], true);
f(3, 2.3, {a: 42});
f(undefined, null, Symbol('hello'));
`;

let {session, contextGroup, Protocol} = InspectorTest.start("Test collecting type profile data with Profiler.takeTypeProfile.");

(async function testTypeProfile() {
  await Protocol.Profiler.enable();
  await Protocol.Profiler.startTypeProfile();

  Protocol.Runtime.enable();
  let {result: {scriptId}} = await Protocol.Runtime.compileScript({
    expression: source,
    sourceURL: arguments.callee.name,
    persistScript: true
  });
  Protocol.Runtime.runScript({ scriptId });

  let typeProfiles = await Protocol.Profiler.takeTypeProfile();
  await session.logTypeProfile(typeProfiles.result.result[0],
    source);

  Protocol.Profiler.stoptTypeProfile();
  Protocol.Profiler.disable();
  await Protocol.Runtime.disable();
  InspectorTest.completeTest();
})();
