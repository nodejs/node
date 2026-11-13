// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strict-termination-checks --js-defer-import-eval

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests termination handling during async module rejection and promise ' +
    'rejection.');

function makeTerminatingErrorExpr(message) {
  return `(() => {
    let shouldPause = true;
    const err = new Error(${JSON.stringify(message)});
    Object.setPrototypeOf(err, {
      get name() {
        if (!shouldPause) return 'Error';
        shouldPause = false;
        debugger;
        for (;;) {}
      }
    });
    return err;
  })()`;
}

InspectorTest.runAsyncTestSuite([
  async function setUp() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
  },

  async function testAsyncModuleParentChainTermination() {
    contextGroup.addModule(
        'await new Promise((_, r) => { globalThis.rejectChainChild = r; });',
        'chain-child.js');
    contextGroup.addModule('import "chain-child.js";', 'chain-parent.js');

    let exceptions = 0;
    Protocol.Runtime.onExceptionThrown(({params: {exceptionDetails}}) => {
      exceptions++;
      InspectorTest.log(
          `chain exceptionThrown #${exceptions}: ${exceptionDetails.text}`);
    });

    const paused = Protocol.Debugger.oncePaused();
    const evaluated = Protocol.Runtime.evaluate({
      expression: `globalThis.rejectChainChild(${
          makeTerminatingErrorExpr('chain-boom')});`
    });
    await paused;
    Protocol.Runtime.terminateExecution();
    await Protocol.Debugger.resume();
    await evaluated;
    InspectorTest.log(`chain exceptions after termination: ${exceptions}`);

    // Re-importing chain-parent.js after termination must still observe the
    // original evaluation error rather than null.
    contextGroup.addModule('import "chain-parent.js";', 'chain-reimport.js');
    await InspectorTest.waitForPendingTasks();
    Protocol.Runtime.onExceptionThrown(null);
  },

  async function testAsyncModuleSiblingParentsTermination() {
    contextGroup.addModuleWithoutEvaluating(
        'await new Promise((_, r) => { globalThis.rejectDiamondChild = r; });',
        'diamond-child.js');
    contextGroup.addModule('import "diamond-child.js";', 'diamond-parent1.js');
    contextGroup.addModule('import "diamond-child.js";', 'diamond-parent2.js');

    let exceptions = 0;
    Protocol.Runtime.onExceptionThrown(({params: {exceptionDetails}}) => {
      exceptions++;
      InspectorTest.log(
          `diamond exceptionThrown #${exceptions}: ${exceptionDetails.text}`);
    });

    const paused = Protocol.Debugger.oncePaused();
    const evaluated = Protocol.Runtime.evaluate({
      expression: `globalThis.rejectDiamondChild(${
          makeTerminatingErrorExpr('diamond-boom')});`
    });
    await paused;
    Protocol.Runtime.terminateExecution();
    await Protocol.Debugger.resume();
    await evaluated;
    InspectorTest.log(`diamond exceptions after termination: ${exceptions}`);

    // Re-importing diamond-parent2.js after termination must still observe the
    // original evaluation error rather than null.
    contextGroup.addModule(
        'import "diamond-parent2.js";', 'diamond-reimport.js');
    await InspectorTest.waitForPendingTasks();
    Protocol.Runtime.onExceptionThrown(null);
  },

  async function testImportDeferModuleTermination() {
    // 1. SourceTextModule::Evaluate synchronous error rejection path: user
    //    try/catch around deferred namespace access must not swallow
    //    termination triggered inside JSPromise::Reject.
    await Protocol.Runtime.evaluate({
      expression: `globalThis.deferSyncErr = ${
          makeTerminatingErrorExpr('defer-sync-boom')}; undefined;`
    });
    contextGroup.addModuleWithoutEvaluating(
        'export const x = 1; throw globalThis.deferSyncErr;', 'defer-sync.js');
    contextGroup.addModule(
        'import defer * as ns from "defer-sync.js"; ' +
            'globalThis.accessDeferSync = () => ns.x;',
        'defer-sync-importer.js');
    await InspectorTest.waitForPendingTasks();

    const pausedSync = Protocol.Debugger.oncePaused();
    const evaluatedSync = Protocol.Runtime.evaluate({
      expression: `
        globalThis.ranAfterDeferSync = false;
        try { globalThis.accessDeferSync(); } catch {}
        globalThis.ranAfterDeferSync = true;
      `
    });
    await pausedSync;
    Protocol.Runtime.terminateExecution();
    await Protocol.Debugger.resume();
    await evaluatedSync;
    const {result: {result: {value: ranAfterDeferSync}}} =
        await Protocol.Runtime.evaluate({
          expression: 'globalThis.ranAfterDeferSync',
          returnByValue: true,
        });
    InspectorTest.log(`ran after defer sync eval: ${ranAfterDeferSync}`);

    // 2. Module::Evaluate kEvaluatedError path (already-errored dependency with
    //    empty top_level_capability): user try/catch must not swallow
    //    termination triggered inside JSPromise::Reject.
    await Protocol.Runtime.evaluate({
      expression: `{
        globalThis.armDeferLeaf = false;
        const err = new Error('defer-leaf-boom');
        Object.setPrototypeOf(err, {
          get name() {
            if (!globalThis.armDeferLeaf) return 'Error';
            globalThis.armDeferLeaf = false;
            debugger;
            for (;;) {}
          }
        });
        globalThis.deferLeafErr = err;
      }`
    });
    contextGroup.addModuleWithoutEvaluating(
        'export const x = 1; throw globalThis.deferLeafErr;', 'defer-leaf.js');
    contextGroup.addModule('import "defer-leaf.js";', 'defer-leaf-parent.js');
    contextGroup.addModule(
        'import defer * as ns from "defer-leaf.js"; ' +
            'globalThis.accessDeferLeaf = () => ns.x;',
        'defer-leaf-importer.js');
    await InspectorTest.waitForPendingTasks();
    await Protocol.Runtime.evaluate(
        {expression: 'globalThis.armDeferLeaf = true;'});

    const pausedLeaf = Protocol.Debugger.oncePaused();
    const evaluatedLeaf = Protocol.Runtime.evaluate({
      expression: `
        globalThis.ranAfterDeferLeaf = false;
        try { globalThis.accessDeferLeaf(); } catch {}
        globalThis.ranAfterDeferLeaf = true;
      `
    });
    await pausedLeaf;
    Protocol.Runtime.terminateExecution();
    await Protocol.Debugger.resume();
    await evaluatedLeaf;
    const {result: {result: {value: ranAfterDeferLeaf}}} =
        await Protocol.Runtime.evaluate({
          expression: 'globalThis.ranAfterDeferLeaf',
          returnByValue: true,
        });
    InspectorTest.log(`ran after defer errored leaf: ${ranAfterDeferLeaf}`);
  },

  async function testPromiseRejectTermination() {
    const cases = [
      ['Promise.reject', 'Promise.reject(err)'],
      ['reject callback', 'new Promise((_, reject) => reject(err))'],
      [
        'Promise.resolve then getter',
        'Promise.resolve({ get then() { throw err; } })'
      ],
      [
        'AsyncDisposableStack.disposeAsync',
        '(() => { const s = new AsyncDisposableStack(); ' +
            's.defer(() => { throw err; }); return s.disposeAsync(); })()'
      ],
      [
        'AsyncDisposeFromSyncDispose',
        '(() => { const s = new AsyncDisposableStack(); ' +
            's.use({ [Symbol.dispose]() { throw err; } }); ' +
            'return s.disposeAsync(); })()'
      ],
    ];
    for (const [label, rejectExpr] of cases) {
      const paused = Protocol.Debugger.oncePaused();
      const evaluated = Protocol.Runtime.evaluate({
        expression: `
          globalThis.ranAfterReject = false;
          {
            const err = ${makeTerminatingErrorExpr('reject-boom')};
            ${rejectExpr};
          }
          globalThis.ranAfterReject = true;
        `
      });
      await paused;
      Protocol.Runtime.terminateExecution();
      await Protocol.Debugger.resume();
      await evaluated;
      const {result: {result: {value: ranAfterReject}}} =
          await Protocol.Runtime.evaluate({
            expression: 'globalThis.ranAfterReject',
            returnByValue: true,
          });
      InspectorTest.log(`ran after ${label}: ${ranAfterReject}`);
    }
  },
]);
