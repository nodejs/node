import * as common from '../common/index.mjs';

import assert from 'node:assert';
import { spawn, exec } from 'node:child_process';


{
  const lines = exec(`${process.execPath} -p 42`).readLines();

  lines.on('line', common.mustCall((result) => {
    assert.strictEqual(result, '42');
    lines.on('line', common.mustNotCall('We expect only one line event'));
  }));
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

  [0, 1, '', 'a', 0n, 1n, Symbol(), () => {}, {}, []].forEach((listenTo) => assert.throws(
    () => cp.readLines({ listenTo }),
    { code: 'ERR_INVALID_ARG_VALUE' }));

  [0, 1, '', 'a', 0n, 1n, Symbol(), () => {}, {}, []].forEach((rejectIfNonZeroExitCode) => assert.throws(
    () => cp.readLines({ rejectIfNonZeroExitCode }),
    { code: 'ERR_INVALID_ARG_TYPE' }));
}

await assert.rejects(async () => {
  // eslint-disable-next-line no-unused-vars
  for await (const _ of spawn(process.execPath, ['-p', 42], { signal: AbortSignal.abort() }).readLines());
}, { name: 'AbortError' });


await assert.rejects(async () => {
  // eslint-disable-next-line no-unused-vars
  for await (const _ of spawn(process.execPath, { signal: AbortSignal.abort() }).readLines());
}, { name: 'AbortError' });

{
  const ac = new AbortController();
  const cp = spawn(
    process.execPath,
    ['-e', 'setTimeout(()=>console.log("line 2"), 10);setImmediate(()=>console.log("line 1"));'],
    { signal: ac.signal });
  await assert.rejects(async () => {
    for await (const line of cp.readLines()) {
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
    for await (const _ of cp.readLines({ rejectIfNonZeroExitCode: true }));
  }, { pid: cp.pid, status: 1, signal: null });
}


{
  const fn = common.mustCall();
  for await (const line of spawn(process.execPath, ['-e', 'console.error(42)']).readLines({ listenTo: 'stderr' })) {
    assert.strictEqual(line, '42');
    fn();
  }
}

{
  const stderr = (await spawn(process.execPath, ['-e', 'throw new Error']).stderr.toArray()).join('').split(/\r?\n/);
  const cp = spawn(process.execPath, ['-e', 'throw new Error']);
  assert.strictEqual(cp.exitCode, null);
  for await (const line of cp.readLines({ listenTo: 'stderr' })) {
    assert.strictEqual(line, stderr.shift());
  }
  assert.strictEqual(cp.exitCode, 1);
  assert.deepStrictEqual(stderr, ['']);
}
