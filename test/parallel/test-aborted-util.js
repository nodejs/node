// Flags: --expose-gc
'use strict';

const common = require('../common');
const { aborted } = require('util');
const assert = require('assert');
const { getEventListeners } = require('events');
const { spawn } = require('child_process');

{
  // Test aborted works when provided a resource
  const ac = new AbortController();
  aborted(ac.signal, {}).then(common.mustCall());
  ac.abort();
  assert.strictEqual(ac.signal.aborted, true);
  assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 0);
}

{
  // Test aborted with gc cleanup
  const ac = new AbortController();
  aborted(ac.signal, {}).then(common.mustNotCall());
  setImmediate(() => {
    global.gc();
    ac.abort();
    assert.strictEqual(ac.signal.aborted, true);
    assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 0);
  });
}

{
  // Fails with error if not provided abort signal
  Promise.all([{}, null, undefined, Symbol(), [], 1, 0, 1n, true, false, 'a', () => {}].map((sig) =>
    assert.rejects(aborted(sig, {}), {
      code: 'ERR_INVALID_ARG_TYPE',
    })
  )).then(common.mustCall());
}

{
  // Fails if not provided a resource
  const ac = new AbortController();
  Promise.all([null, undefined, 0, 1, 0n, 1n, Symbol(), '', 'a'].map((resource) =>
    assert.rejects(aborted(ac.signal, resource), {
      code: 'ERR_INVALID_ARG_TYPE',
    })
  )).then(common.mustCall());
}

{
  const childProcess = spawn(process.execPath, ['--input-type=module']);
  childProcess.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 13);
  }));
  childProcess.stdin.end(`
    import { aborted } from 'node:util';
    await aborted(new AbortController().signal, {});
  `);
}
