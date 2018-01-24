// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --type-profile

const source1 =
  `
function g(a, b, c) {
  return 'first';
};
g({}, [], null);
`;

const source2 =
  `
function f(a) {
  return 'second';
};
f(null);
`;

let {session, contextGroup, Protocol} = InspectorTest.start("Turn " +
  "Profiler.startTypeProfile on and off.");

InspectorTest.runAsyncTestSuite([
  async function testTypeProfile() {
    Protocol.Runtime.enable();
    let {result: {scriptId}} = await Protocol.Runtime.compileScript({
      expression: source1,
      sourceURL: arguments.callee.name, persistScript: true
    });
    await Protocol.Profiler.enable();

    // Start, run, take.
    await Protocol.Profiler.startTypeProfile();
    Protocol.Runtime.runScript({scriptId});

    let typeProfiles = await Protocol.Profiler.takeTypeProfile();
    await session.logTypeProfile(typeProfiles.result.result[0],
      source1);

    Protocol.Profiler.stopTypeProfile();
    Protocol.Profiler.disable();
    await Protocol.Runtime.disable();
  },
  async function testTypeProfileFromDifferentSource() {
    Protocol.Runtime.enable();
    let {result: {scriptId}} = await Protocol.Runtime.compileScript({
      expression: source2,
      sourceURL: arguments.callee.name, persistScript: true
    });
    await Protocol.Profiler.enable();

    // Start, run different script, take.
    await Protocol.Profiler.startTypeProfile();
    Protocol.Runtime.runScript({scriptId});

    let typeProfiles = await Protocol.Profiler.takeTypeProfile();
    await session.logTypeProfile(typeProfiles.result.result[0],
      source2);

    Protocol.Profiler.stopTypeProfile();
    Protocol.Profiler.disable();
    await Protocol.Runtime.disable();
  },
  async function testStopTypeProfileDeletesFeedback() {
    Protocol.Runtime.enable();
    let {result: {scriptId}} = await Protocol.Runtime.compileScript({
      expression: source1,
      sourceURL: arguments.callee.name, persistScript: true
    });
    await Protocol.Profiler.enable();

    // Start, run, stop.
    await Protocol.Profiler.startTypeProfile();
    Protocol.Runtime.runScript({scriptId});
    await Protocol.Profiler.stopTypeProfile();

    // Start, take. Should be empty, because no code was run.
    await Protocol.Profiler.startTypeProfile();
    let typeProfiles = await Protocol.Profiler.takeTypeProfile();
    InspectorTest.logMessage(typeProfiles.result.result);
    await Protocol.Profiler.stopTypeProfile();

    Protocol.Profiler.disable();
    await Protocol.Runtime.disable();
  },
  async function testTypeProfileWithoutStartingItFirst() {
    Protocol.Runtime.enable();
    let {result: {scriptId}} = await Protocol.Runtime.compileScript({ expression: source1,
      sourceURL: arguments.callee.name, persistScript: true });
    Protocol.Runtime.runScript({ scriptId });
    await Protocol.Profiler.enable();

    // This should return an error because type profile was never started.
    let typeProfiles = await Protocol.Profiler.takeTypeProfile();
    InspectorTest.logObject(typeProfiles.error.message);

    Protocol.Profiler.disable();
    await Protocol.Runtime.disable();
  },
  async function testTypeProfileAfterStoppingIt() {
    Protocol.Runtime.enable();
    let {result: {scriptId}} = await Protocol.Runtime.compileScript({ expression: source1,
      sourceURL: arguments.callee.name, persistScript: true });
    Protocol.Runtime.runScript({ scriptId });
    await Protocol.Profiler.enable();
    await Protocol.Profiler.startTypeProfile();

    // Make sure that this turns off type profile.
    await Protocol.Profiler.stopTypeProfile();

    // This should return an error because type profile was stopped.
    let typeProfiles = await Protocol.Profiler.takeTypeProfile();
    InspectorTest.logObject(typeProfiles.error.message);

    Protocol.Profiler.disable();
    await Protocol.Runtime.disable();
  },
  async function testStartTypeProfileAfterRunning() {
    Protocol.Runtime.enable();
    let {result: {scriptId}} = await Protocol.Runtime.compileScript({
      expression: source1,
      sourceURL: arguments.callee.name, persistScript: true
    });
    Protocol.Runtime.runScript({scriptId});

    await Protocol.Profiler.enable();
    await Protocol.Profiler.startTypeProfile();

    let typeProfiles = await Protocol.Profiler.takeTypeProfile();

    // This should be empty because type profile was started after compilation.
    // Only the outer script is annotated with return value "string" because
    // that does not depend on runScript().
    InspectorTest.logMessage(typeProfiles);

    Protocol.Profiler.stopTypeProfile();
    Protocol.Profiler.disable();
    await Protocol.Runtime.disable();
  },
  async function testTypeProfileForTwoSources() {
    Protocol.Runtime.enable();
    let {result: {scriptId: scriptId1}} = await Protocol.Runtime.compileScript({
      expression: source1,
      sourceURL: arguments.callee.name, persistScript: true
    });
    let {result: {scriptId: scriptId2}} = await Protocol.Runtime.compileScript({
      expression: source2,
      sourceURL: arguments.callee.name, persistScript: true
    });
    await Protocol.Profiler.enable();

    // Start, run different script, take.
    await Protocol.Profiler.startTypeProfile();
    Protocol.Runtime.runScript({scriptId: scriptId1});
    Protocol.Runtime.runScript({scriptId: scriptId2});

    let typeProfiles = await Protocol.Profiler.takeTypeProfile();
    await session.logTypeProfile(typeProfiles.result.result[0],
      source1);
    await session.logTypeProfile(typeProfiles.result.result[1],
      source2);

    Protocol.Profiler.stopTypeProfile();
    Protocol.Profiler.disable();
    await Protocol.Runtime.disable();
  },
  async function testStopTwice() {
    Protocol.Runtime.enable();
    await Protocol.Profiler.enable();
    await Protocol.Profiler.stopTypeProfile();
    await Protocol.Profiler.stopTypeProfile();
    Protocol.Profiler.disable();
    await Protocol.Runtime.disable();
  },
]);
