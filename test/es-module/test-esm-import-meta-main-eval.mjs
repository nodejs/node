import { escapePOSIXShell, mustSucceed } from '../common/index.mjs';
import child from 'node:child_process';

const execOptions = '--input-type module';

const script = `
import { mustCall } from '../common/index.mjs';
import assert from 'node:assert/strict';

assert.strictEqual(import.meta.main, true);

import('../fixtures/es-modules/import-meta-main.mjs').then(
  mustCall(({ isMain }) => {
    assert.strictEqual(isMain, false);
  })
);
`

const scriptInWorker = `
import { Worker } from 'node:worker_threads';
new Worker(\`${script}\`, { eval: true });
`

function test_evaluated_script() {
  let [cmd, opts] = escapePOSIXShell`"${process.execPath}" ${execOptions} --eval "${script}"`;
  opts.cwd = import.meta.dirname
  child.exec(cmd, opts, mustSucceed((stdout) => {}));
}

function test_evaluated_worker_script() {
  let [cmd, opts] = escapePOSIXShell`"${process.execPath}" ${execOptions} --eval "${scriptInWorker}"`;
  opts.cwd = import.meta.dirname
  child.exec(cmd, opts, mustSucceed((stdout) => {}));
}

//test_evaluated_script()
test_evaluated_worker_script()
