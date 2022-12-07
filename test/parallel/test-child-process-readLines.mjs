import * as common from '../common/index.mjs';

import assert from 'node:assert';
import { spawn, exec, execSync } from 'node:child_process';


{
  const lines = exec(`${process.execPath} -p 42`).readLines();

  lines.on('line', common.mustCall((result) => assert.strictEqual(result, '42')));
}

{
  const fn = common.mustCall();
  for await (const line of spawn(process.execPath, ['-p', 42]).readLines()) {
    assert.strictEqual(line, '42');
    fn();
  }
}

{
  const cp = spawn(process.execPath, ['-p', 42]);

  [0, 1, '', 'a', 0n, 1n, Symbol(), () => {}, {}, []].forEach((useStdErr) => assert.throws(
    () => cp.readLines({ useStdErr }),
    { code: 'ERR_INVALID_ARG_TYPE' }));

  [0, 1, '', 'a', 0n, 1n, Symbol(), () => {}, {}, []].forEach((ignoreErrors) => assert.throws(
    () => cp.readLines({ ignoreErrors }),
    { code: 'ERR_INVALID_ARG_TYPE' }));
}

await assert.rejects(async () => {
  // eslint-disable-next-line no-unused-vars
  for await (const _ of spawn(process.execPath, ['-p', 42], { signal: AbortSignal.abort() }).readLines());
}, { name: 'AbortError' });


await assert.rejects(async () => {
  // eslint-disable-next-line no-unused-vars
  for await (const _ of spawn(process.execPath, { signal: AbortSignal.abort() }).readLines({ ignoreErrors: true }));
}, { name: 'AbortError' });

{
  const ac = new AbortController();
  const cp = spawn(
    process.execPath,
    ['-e', 'setTimeout(()=>console.log("line 2"), 10);setImmediate(()=>console.log("line 1"));'],
    { signal: ac.signal });
  await assert.rejects(async () => {
    for await (const line of cp.readLines({ ignoreErrors: true })) {
      assert.strictEqual(line, 'line 1');
      ac.abort();
    }
  }, { name: 'AbortError' });
  assert.strictEqual(ac.signal.aborted, true);
}

{
  const cp = spawn(process.execPath, ['-e', 'throw null']);
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of cp.readLines());
  }, { pid: cp.pid, status: 1, signal: null });
}


{
  const fn = common.mustCall();
  for await (const line of spawn(process.execPath, ['-e', 'console.error(42)']).readLines({ useStdErr: true })) {
    assert.strictEqual(line, '42');
    fn();
  }
}

{
  let stderr;
  try {
    execSync(`${process.execPath} -e 'throw new Error'`, { stdio: 'pipe', encoding: 'utf-8' });
  } catch (err) {
    if (!Array.isArray(err?.output)) throw err;
    stderr = err.output[2].split(/\r?\n/);
  }
  const cp = spawn(process.execPath, ['-e', 'throw new Error']);
  for await (const line of cp.readLines({ useStdErr: true, ignoreErrors: true })) {
    assert.strictEqual(line, stderr.shift());
  }
  assert.deepStrictEqual(stderr, ['']);
}
