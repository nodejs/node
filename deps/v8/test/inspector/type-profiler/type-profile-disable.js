// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --type-profile

const source =
  `
function g(a, b, c) {
  return 'bye';
};
g({}, [], null);
`;

let {session, contextGroup, Protocol} = InspectorTest.start("Turn " +
  "Profiler.startTypeProfile on and off.");

(async function testTypeProfile() {
  Protocol.Runtime.enable();
   let {result: {scriptId}} = await Protocol.Runtime.compileScript({
    expression: source,
    sourceURL: arguments.callee.name, persistScript: true
  });
  await Protocol.Profiler.enable();
  // Start, run, take.
  await Protocol.Profiler.startTypeProfile();
  Protocol.Runtime.runScript({scriptId});

  let typeProfiles = await Protocol.Profiler.takeTypeProfile();
  session.logTypeProfile(typeProfiles.result.result[0],
    source);

  // This should delete all data.
  Protocol.Profiler.stopTypeProfile();

  await Protocol.Profiler.startTypeProfile();
  typeProfiles = await Protocol.Profiler.takeTypeProfile();

  // Should be empty because no code was run since start.
  InspectorTest.logMessage(typeProfiles.result.result);

  Protocol.Profiler.stopTypeProfile();

  Protocol.Profiler.disable();
  await Protocol.Runtime.disable();
  InspectorTest.completeTest();
})();
