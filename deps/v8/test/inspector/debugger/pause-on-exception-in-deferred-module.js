// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that an exception thrown while evaluating a deferred module ' +
    'pauses exactly once, at the original throw site, and only when its ' +
    'predicted catch state matches the pause mode. Rethrowing a cached ' +
    'evaluation error pauses once at the access site.');

const scripts = new Map();
Protocol.Debugger.onScriptParsed(
    ({params}) => scripts.set(params.scriptId, params.url));

Protocol.Debugger.onPaused(({params: {callFrames, reason, data}}) => {
  InspectorTest.log(`paused: reason=${reason} uncaught=${data.uncaught}`);
  for (const {functionName, location} of callFrames) {
    const {scriptId, lineNumber, columnNumber} = location;
    InspectorTest.log(`    at ${functionName || '<top>'} ` +
        `(${scripts.get(scriptId)}:${lineNumber}:${columnNumber})`);
  }
  Protocol.Debugger.resume();
});

const kThrowingBody = 'throw new Error("boom");\nexport const x = 1;';

// Each case returns the steps to run for a given unique `id`. Steps in
// `setup` run with pauses disabled; steps in `run` run with the pause mode
// under test.
const cases = [
  {
    name: 'access inside try/catch',
    steps: (id) => ({
      run: [
        ['moduleWithoutEvaluating', kThrowingBody, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\n` +
                   'try { ns.x; } catch (e) {}', `main-${id}`],
      ],
    }),
  },
  {
    name: 'access not caught',
    steps: (id) => ({
      run: [
        ['moduleWithoutEvaluating', kThrowingBody, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\nns.x;`,
         `main-${id}`],
      ],
    }),
  },
  {
    name: 'access inside try/finally',
    steps: (id) => ({
      run: [
        ['moduleWithoutEvaluating', kThrowingBody, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\n` +
                   'try { ns.x; } finally {}', `main-${id}`],
      ],
    }),
  },
  {
    name: 'throw inside a function called by the module body',
    steps: (id) => ({
      run: [
        ['moduleWithoutEvaluating',
         'function thrower() {\n  throw new Error("boom");\n}\n' +
         'thrower();\nexport const x = 1;', `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\n` +
                   'try { ns.x; } catch (e) {}', `main-${id}`],
      ],
    }),
  },
  {
    name: 'throw in a dependency of the deferred module',
    steps: (id) => ({
      run: [
        ['moduleWithoutEvaluating', kThrowingBody, `inner-${id}`],
        ['moduleWithoutEvaluating',
         `import "inner-${id}";\nexport const x = 1;`, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\n` +
                   'try { ns.x; } catch (e) {}', `main-${id}`],
      ],
    }),
  },
  {
    name: 'access from a classic script inside try/catch',
    steps: (id) => ({
      setup: [
        ['moduleWithoutEvaluating', kThrowingBody, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\n` +
                   `globalThis["ns-${id}"] = ns;`, `main-${id}`],
      ],
      run: [
        ['script', `try { globalThis["ns-${id}"].x; } catch (e) {}`,
         `script-${id}`],
      ],
    }),
  },
  {
    name: 'access from a classic script not caught',
    steps: (id) => ({
      setup: [
        ['moduleWithoutEvaluating', kThrowingBody, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\n` +
                   `globalThis["ns-${id}"] = ns;`, `main-${id}`],
      ],
      run: [
        ['script', `globalThis["ns-${id}"].x;`, `script-${id}`],
      ],
    }),
  },
  {
    name: 'access from an async function inside try/catch',
    steps: (id) => ({
      run: [
        ['moduleWithoutEvaluating', kThrowingBody, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\n` +
                   'async function f() {\n' +
                   '  await 0;\n' +
                   '  try { ns.x; } catch (e) {}\n' +
                   '}\n' +
                   'f();', `main-${id}`],
      ],
    }),
  },
  {
    name: 'access from an async function not caught',
    steps: (id) => ({
      run: [
        ['moduleWithoutEvaluating', kThrowingBody, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\n` +
                   'async function f() {\n' +
                   '  await 0;\n' +
                   '  ns.x;\n' +
                   '}\n' +
                   'f();', `main-${id}`],
      ],
    }),
  },
  {
    name: 'second access inside try/catch rethrows the cached error',
    steps: (id) => ({
      setup: [
        ['moduleWithoutEvaluating', kThrowingBody, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\n` +
                   'try { ns.x; } catch (e) {}\n' +
                   `globalThis["ns-${id}"] = ns;`, `main-${id}`],
      ],
      run: [
        ['script', `try { globalThis["ns-${id}"].x; } catch (e) {}`,
         `script-${id}`],
      ],
    }),
  },
  {
    name: 'second access not caught rethrows the cached error',
    steps: (id) => ({
      setup: [
        ['moduleWithoutEvaluating', kThrowingBody, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\n` +
                   'try { ns.x; } catch (e) {}\n' +
                   `globalThis["ns-${id}"] = ns;`, `main-${id}`],
      ],
      run: [
        ['script', `globalThis["ns-${id}"].x;`, `script-${id}`],
      ],
    }),
  },
  {
    name: 'dependency already errored, access inside try/catch',
    steps: (id) => ({
      setup: [
        ['moduleWithoutEvaluating', kThrowingBody, `inner-${id}`],
        ['module', `import defer * as ns from "inner-${id}";\n` +
                   'try { ns.x; } catch (e) {}', `first-${id}`],
      ],
      run: [
        ['moduleWithoutEvaluating',
         `import "inner-${id}";\nexport const x = 1;`, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\n` +
                   'try { ns.x; } catch (e) {}', `main-${id}`],
      ],
    }),
  },
  {
    name: 'dependency already errored, access not caught',
    steps: (id) => ({
      setup: [
        ['moduleWithoutEvaluating', kThrowingBody, `inner-${id}`],
        ['module', `import defer * as ns from "inner-${id}";\n` +
                   'try { ns.x; } catch (e) {}', `first-${id}`],
      ],
      run: [
        ['moduleWithoutEvaluating',
         `import "inner-${id}";\nexport const x = 1;`, `dep-${id}`],
        ['module', `import defer * as ns from "dep-${id}";\nns.x;`,
         `main-${id}`],
      ],
    }),
  },
];

async function runSteps(steps) {
  for (const [kind, source, url] of steps) {
    switch (kind) {
      case 'moduleWithoutEvaluating':
        contextGroup.addModuleWithoutEvaluating(source, url);
        break;
      case 'module':
        contextGroup.addModule(source, url);
        break;
      case 'script':
        contextGroup.addScript(source, 0, 0, url);
        break;
    }
  }
  await InspectorTest.waitForPendingTasks();
}

InspectorTest.runAsyncTestSuite([
  async function testPauseOnExceptions() {
    await Protocol.Debugger.enable();
    let caseIndex = 0;
    for (const {name, steps} of cases) {
      for (const state of ['uncaught', 'caught', 'all']) {
        InspectorTest.log(`\n--- ${name}, pauseOnExceptions=${state}`);
        const {setup = [], run} = steps(`${caseIndex}-${state}`);
        await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
        await runSteps(setup);
        await Protocol.Debugger.setPauseOnExceptions({state});
        await runSteps(run);
      }
      ++caseIndex;
    }
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
    await Protocol.Debugger.disable();
  },
]);
