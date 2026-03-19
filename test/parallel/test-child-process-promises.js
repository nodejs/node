// Flags: --no-warnings
'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');

const { exec, execFile } = require('child_process/promises');

// Delegated functions are the same as util.promisify(child_process.exec/execFile).
{
  const { promisify } = require('util');
  assert.strictEqual(exec, promisify(child_process.exec));
  assert.strictEqual(execFile, promisify(child_process.execFile));
}

// exec resolves with { stdout, stderr }.
{
  const promise = exec(...common.escapePOSIXShell`"${process.execPath}" -p 42`);

  assert(promise.child instanceof child_process.ChildProcess);
  promise.then(common.mustCall((result) => {
    assert.deepStrictEqual(result, { stdout: '42\n', stderr: '' });
  }));
}

// execFile resolves with { stdout, stderr }.
{
  const promise = execFile(process.execPath, ['-p', '42']);

  assert(promise.child instanceof child_process.ChildProcess);
  promise.then(common.mustCall((result) => {
    assert.deepStrictEqual(result, { stdout: '42\n', stderr: '' });
  }));
}

// exec rejects when command does not exist.
{
  const promise = exec('doesntexist');

  assert(promise.child instanceof child_process.ChildProcess);
  promise.catch(common.mustCall((err) => {
    assert(err.message.includes('doesntexist'));
  }));
}

// execFile rejects when file does not exist.
{
  const promise = execFile('doesntexist', ['-p', '42']);

  assert(promise.child instanceof child_process.ChildProcess);
  promise.catch(common.mustCall((err) => {
    assert(err.message.includes('doesntexist'));
  }));
}

// Rejected errors include stdout, stderr, and code properties.
const failingCodeWithStdoutErr =
  'console.log(42);console.error(43);process.exit(1)';

{
  exec(...common.escapePOSIXShell`"${process.execPath}" -e "${failingCodeWithStdoutErr}"`)
    .catch(common.mustCall((err) => {
      assert.strictEqual(err.code, 1);
      assert.strictEqual(err.stdout, '42\n');
      assert.strictEqual(err.stderr, '43\n');
    }));
}

{
  execFile(process.execPath, ['-e', failingCodeWithStdoutErr])
    .catch(common.mustCall((err) => {
      assert.strictEqual(err.code, 1);
      assert.strictEqual(err.stdout, '42\n');
      assert.strictEqual(err.stderr, '43\n');
    }));
}

// execFile with options but no args array.
{
  execFile(process.execPath, { timeout: 5000 })
    .catch(common.mustCall(() => {
      // Expected to fail (no script), but should not throw synchronously.
    }));
}

// encoding option returns strings.
{
  execFile(process.execPath, ['-p', '"hello"'], { encoding: 'utf8' })
    .then(common.mustCall((result) => {
      assert.strictEqual(typeof result.stdout, 'string');
      assert.strictEqual(typeof result.stderr, 'string');
      assert.strictEqual(result.stdout, 'hello\n');
    }));
}

// encoding 'buffer' returns Buffer instances.
{
  execFile(process.execPath, ['-p', '"hello"'], { encoding: 'buffer' })
    .then(common.mustCall((result) => {
      assert(Buffer.isBuffer(result.stdout));
      assert(Buffer.isBuffer(result.stderr));
      assert.strictEqual(result.stdout.toString(), 'hello\n');
    }));
}

// AbortSignal cancels exec.
{
  const waitCommand = common.isWindows ?
    `"${process.execPath}" -e "setInterval(()=>{}, 99)"` :
    'sleep 2m';
  const ac = new AbortController();
  const promise = exec(waitCommand, { signal: ac.signal });

  assert.rejects(promise, {
    name: 'AbortError',
  }).then(common.mustCall());
  ac.abort();
}

// AbortSignal cancels execFile.
{
  const ac = new AbortController();
  const promise = execFile(
    process.execPath,
    ['-e', 'setInterval(()=>{}, 99)'],
    { signal: ac.signal },
  );

  assert.rejects(promise, {
    name: 'AbortError',
  }).then(common.mustCall());
  ac.abort();
}

// Already-aborted signal rejects immediately.
{
  const signal = AbortSignal.abort();
  const promise = exec('echo hello', { signal });

  assert.rejects(promise, { name: 'AbortError' })
    .then(common.mustCall());
}

// maxBuffer causes rejection.
{
  execFile(
    process.execPath,
    ['-e', "console.log('a'.repeat(100))"],
    { maxBuffer: 10 },
  ).catch(common.mustCall((err) => {
    assert(err instanceof RangeError);
    assert.strictEqual(err.code, 'ERR_CHILD_PROCESS_STDIO_MAXBUFFER');
  }));
}

// timeout causes the child process to be killed.
{
  const promise = execFile(
    process.execPath,
    ['-e', 'setInterval(()=>{}, 99)'],
    { timeout: 1 },
  );

  promise.catch(common.mustCall((err) => {
    assert.ok(err.killed || err.signal);
  }));
}

// Module can be loaded with node: scheme.
{
  const promises = require('node:child_process/promises');
  assert.strictEqual(typeof promises.exec, 'function');
  assert.strictEqual(typeof promises.execFile, 'function');
}
