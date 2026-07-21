// Verifies that type-stripped TypeScript reports a file: URL as its script
// sourceURL to the inspector across all module loading paths, hinting that it
// is a generated source.
'use strict';

const common = require('../common');
const { describe, it } = require('node:test');
common.skipIfInspectorDisabled();
if (!process.config.variables.node_use_amaro) common.skip('Requires Amaro');

const { NodeInstance } = require('../common/inspector-helper.js');
const fixtures = require('../common/fixtures');
const assert = require('assert');

const scenarios = [
  {
    // CommonJS: a .ts entry point.
    entry: 'typescript/ts/test-typescript.ts',
    expected: [
      fixtures.fileURL('typescript/ts/test-typescript.ts').href,
    ],
  },
  {
    // CommonJS: a .ts entry that require()s a .cts dependency.
    entry: 'typescript/ts/test-require-cts.ts',
    expected: [
      fixtures.fileURL('typescript/ts/test-require-cts.ts').href,
      fixtures.fileURL('typescript/cts/test-cts-export-foo.cts').href,
    ],
  },
  {
    // CommonJS: a .ts entry that require()s a .mts dependency.
    entry: 'typescript/ts/test-require-mts.ts',
    expected: [
      fixtures.fileURL('typescript/ts/test-require-mts.ts').href,
      fixtures.fileURL('typescript/mts/test-mts-export-foo.mts').href,
    ],
  },
  {
    // ESM: a .mts entry that imports a .ts dependency.
    entry: 'typescript/mts/test-import-ts-file.mts',
    expected: [
      fixtures.fileURL('typescript/mts/test-import-ts-file.mts').href,
      fixtures.fileURL('typescript/mts/test-module-export.ts').href,
    ],
  },
  {
    // ESM: a .mts entry that imports a .cts dependency.
    entry: 'typescript/mts/test-import-commonjs.mts',
    expected: [
      fixtures.fileURL('typescript/mts/test-import-commonjs.mts').href,
      fixtures.fileURL('typescript/cts/test-cts-export-foo.cts').href,
    ],
  },
];

async function collectParsedScripts(scriptPath) {
  const child = new NodeInstance(['--inspect-brk=0'], undefined, scriptPath);
  const session = await child.connectInspectorSession();

  await session.send({ method: 'NodeRuntime.enable' });
  await session.waitForNotification('NodeRuntime.waitingForDebugger');
  await session.send([
    { 'method': 'Debugger.enable' },
    { 'method': 'Runtime.enable' },
    { 'method': 'Runtime.runIfWaitingForDebugger' },
  ]);
  await session.send({ method: 'NodeRuntime.disable' });

  // Collect every parsed script while resuming through the break on start and
  // any later pauses, until the main execution context is destroyed.
  const scripts = new Map();
  await session.waitForNotification((notification) => {
    if (notification.method === 'Debugger.scriptParsed') {
      const { url } = notification.params;
      if (!url.startsWith('node:')) {
        scripts.set(url, notification.params);
      }
    }
    if (notification.method === 'Debugger.paused') {
      session.send({ method: 'Debugger.resume' });
    }
    return notification.method === 'Runtime.executionContextDestroyed' &&
      notification.params.executionContextId === 1;
  });
  await session.waitForDisconnect();

  assert.strictEqual((await child.expectShutdown()).exitCode, 0);
  return scripts;
}

describe('type stripping source URLs', { concurrency: !process.env.TEST_PARALLEL }, () => {
  for (const { entry, expected } of scenarios) {
    it(entry, async () => {
      const scripts = await collectParsedScripts(fixtures.path(entry));
      for (const href of expected) {
        const params = scripts.get(href);
        assert(params,
               `expected ${entry} to report ${href} to the inspector, ` +
               `got:\n- ${[...scripts.keys()].join('\n- ')}`);
        assert(params.hasSourceURL);
      }
    });
  }
});
