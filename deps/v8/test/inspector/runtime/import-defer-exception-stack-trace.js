// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks the stack trace reported for an uncaught exception thrown while ' +
    'a deferred module is evaluated, with and without async stack traces.');

Protocol.Runtime.onExceptionThrown(({params: {exceptionDetails}}) => {
  const {text, url, lineNumber, columnNumber, stackTrace} = exceptionDetails;
  InspectorTest.log('Runtime.exceptionThrown:');
  InspectorTest.logMessage({text, url, lineNumber, columnNumber, stackTrace});
});
// Evaluating a deferred module rejects the module's internal top-level promise,
// which is reported as an unhandled rejection. EvaluateModuleSync then marks
// that promise as handled (HostPromiseRejectionTracker(promise, "handle")),
// which revokes the report, and throws the error at the access site. Hosts
// that defer reporting until the next microtask checkpoint, as HTML does, never
// show the first report; this test runner reports it immediately.
Protocol.Runtime.onExceptionRevoked(({params: {reason}}) => {
  InspectorTest.log(`Runtime.exceptionRevoked: ${reason}`);
});

const kDeferredModule = `
function thrower() {
  throw new Error('deferred module threw');
}
thrower();
export const foo = 42;
`;

async function testAsyncFunction(suffix) {
  InspectorTest.log('\nAccess from an async function, after an await:');
  contextGroup.addModuleWithoutEvaluating(
      kDeferredModule, `deferred-async-${suffix}.js`);
  contextGroup.addModule(
      `
import defer * as ns from 'deferred-async-${suffix}.js';
async function read() {
  await 0;
  return ns.foo;
}
read();
`,
      `main-async-${suffix}.js`);
  await InspectorTest.waitForPendingTasks();
}

async function testClassicScript(suffix) {
  InspectorTest.log('\nAccess from a classic script:');
  contextGroup.addModuleWithoutEvaluating(
      kDeferredModule, `deferred-script-${suffix}.js`);
  contextGroup.addModule(
      `
import defer * as ns from 'deferred-script-${suffix}.js';
globalThis.ns = ns;
`,
      `main-script-${suffix}.js`);
  await InspectorTest.waitForPendingTasks();
  contextGroup.addScript(
      `
function read() {
  return globalThis.ns.foo;
}
read();
`,
      0, 0, `script-${suffix}.js`);
  await InspectorTest.waitForPendingTasks();
}

(async function test() {
  await Protocol.Runtime.enable();

  InspectorTest.log('\nRunning with async stack traces enabled.');
  await Protocol.Runtime.setAsyncCallStackDepth({maxDepth: 32});
  await testAsyncFunction('enabled');
  await testClassicScript('enabled');

  InspectorTest.log('\nRunning with async stack traces disabled.');
  await Protocol.Runtime.setAsyncCallStackDepth({maxDepth: 0});
  await testAsyncFunction('disabled');
  await testClassicScript('disabled');

  InspectorTest.completeTest();
})();
