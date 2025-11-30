// Flags: --permission --allow-fs-read=* --allow-inspector
'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

const { Session } = require('node:inspector/promises');
const assert = require('node:assert');

const session = new Session();
session.connect();

// Guarantee WorkerImpl doesn't gets bypassed
(async () => {
  await session.post('Debugger.enable');
  await session.post('Runtime.enable');

  globalThis.Worker = require('node:worker_threads').Worker;

  const { result: { objectId } } = await session.post('Runtime.evaluate', { expression: 'Worker' });
  const { internalProperties } = await session.post('Runtime.getProperties', { objectId: objectId });
  const {
    value: {
      value: { scriptId }
    }
  } = internalProperties.filter((prop) => prop.name === '[[FunctionLocation]]')[0];
  const { scriptSource } = await session.post('Debugger.getScriptSource', { scriptId });

  const lineNumber = scriptSource.substring(0, scriptSource.indexOf('new WorkerImpl')).split('\n').length;

  // Attempt to bypass it based on https://hackerone.com/reports/1962701
  await session.post('Debugger.setBreakpointByUrl', {
    lineNumber: lineNumber,
    url: 'node:internal/worker',
    columnNumber: 0,
    condition: '((isInternal = true),false)'
  });

  assert.throws(() => {
    // eslint-disable-next-line no-undef
    new Worker(`
      const child_process = require("node:child_process");
      console.log(child_process.execSync("ls -l").toString());

      console.log(require("fs").readFileSync("/etc/passwd").toString())
    `, {
      eval: true,
    });
  }, {
    message: 'Access to this API has been restricted. Use --allow-worker to manage permissions.',
    code: 'ERR_ACCESS_DENIED',
    permission: 'WorkerThreads'
  });
})().then(common.mustCall());
