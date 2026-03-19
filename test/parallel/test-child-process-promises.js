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

  promise.then(common.mustCall((result) => {
    assert.deepStrictEqual(result, { stdout: '42\n', stderr: '' });
  }));
}

// execFile resolves with { stdout, stderr }.
{
  const promise = execFile(process.execPath, ['-p', '42']);

  promise.then(common.mustCall((result) => {
    assert.deepStrictEqual(result, { stdout: '42\n', stderr: '' });
  }));
}

// exec rejects when command does not exist.
{
  const promise = exec('doesntexist');

  promise.catch(common.mustCall((err) => {
    assert(err.message.includes('doesntexist'));
  }));
}

// execFile rejects when file does not exist.
{
  const promise = execFile('doesntexist', ['-p', '42']);

  promise.catch(common.mustCall((err) => {
    assert(err.message.includes('doesntexist'));
  }));
}

// Rejected errors include stdout, stderr, and code properties.
const failingCodeWithStdoutErr =
  'console.log(42);console.error(43);process.exit(1)';

{
  assert.rejects(exec(...common.escapePOSIXShell`"${process.execPath}" -e "${failingCodeWithStdoutErr}"`),
    {
      code: 1,
      stdout: '42\n',
      stderr: '43\n',
    }).then(common.mustCall());
}

{
  execFile(process.execPath, ['-e', failingCodeWithStdoutErr])
    .catch(common.mustCall((err) => {
      assert.strictEqual(err.code, 1);
      assert.strictEqual(err.stdout, '42\n');
      assert.strictEqual(err.stderr, '43\n');
    }));
}

// execFile with timeout rejects with killed process.
{
  assert.rejects(execFile(process.execPath, ['-e', 'setInterval(()=>{}, 99)'], { timeout: 5 }), {
    killed: true,
  }).then(common.mustCall());
}

// encoding option returns strings.
{
  execFile(process.execPath, ['-p', '"hello"'], { encoding: 'utf8' })
    .then(common.mustCall((result) => {
      assert.deepStrictEqual(result, {
        stdout: 'hello\n',
        stderr: '',
      });
    }));
}

// encoding 'buffer' returns Buffer instances.
{
  execFile(process.execPath, ['-p', '"hello"'], { encoding: 'buffer' })
    .then(common.mustCall((result) => {
      assert.deepStrictEqual(result, {
        stderr: Buffer.from(''),
        stdout: Buffer.from('hello\n'),
      });
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
  assert.rejects(execFile(
    process.execPath,
    ['-e', "console.log('a'.repeat(100))"],
    { maxBuffer: 10 },
  ), {
    name: 'RangeError',
    code: 'ERR_CHILD_PROCESS_STDIO_MAXBUFFER',
  }).then(common.mustCall());
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
  assert.strictEqual(require('node:child_process/promises'), require('child_process/promises'));
}
