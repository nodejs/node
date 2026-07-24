'use strict';

const common = require('../common');

if (!common.isWindows)
  common.skip('Windows-specific: EPERM sharing-violation retry in rmSync');

const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { once } = require('events');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

// UV_FS_O_EXLOCK opens with share mode 0, so deletion fails with EPERM
// until this process kills the child.
const lockerScript = `
  const fs = require('fs');
  const UV_FS_O_EXLOCK = 0x10000000;
  fs.openSync(process.argv[1], fs.constants.O_RDWR | UV_FS_O_EXLOCK);
  process.stdout.write('locked');
  setInterval(() => {}, 60_000);
`;

async function spawnLocker(file) {
  const child = spawn(process.execPath, ['-e', lockerScript, file],
                      { stdio: ['ignore', 'pipe', 'inherit'] });
  const [data] = await once(child.stdout, 'data');
  assert.strictEqual(data.toString(), 'locked');
  return child;
}

// Sleep before retry i is i * retryDelay ms, so all retries take at least
// retryDelay * (1 + 2 + ... + maxRetries) ms.
function minRetryTime({ maxRetries, retryDelay }) {
  return retryDelay * maxRetries * (maxRetries + 1) / 2;
}

function timedRmThrowsEPERM(dir, options) {
  const start = Date.now();
  assert.throws(() => {
    fs.rmSync(dir, { recursive: true, ...options });
  }, {
    code: 'EPERM',
    name: 'Error',
    syscall: 'rm',
  });
  return Date.now() - start;
}

(async () => {
  const dir = tmpdir.resolve('rm-eperm-retries');
  const file = path.join(dir, 'locked.txt');
  fs.mkdirSync(dir);
  fs.writeFileSync(file, 'hello');

  const child = await spawnLocker(file);
  try {
    // Proves the lock is effective: no retries means an immediate EPERM.
    timedRmThrowsEPERM(dir, { maxRetries: 0, retryDelay: 0 });
    assert.strictEqual(fs.existsSync(file), true);

    const options = { maxRetries: 4, retryDelay: 100 };
    const expected = minRetryTime(options);  // 100+200+300+400 = 1000 ms.
    const elapsed = timedRmThrowsEPERM(dir, options);

    // Windows timer granularity may shave a few ms off each Sleep() call.
    const slack = 16 * options.maxRetries;
    assert.ok(elapsed >= expected - slack,
              `rmSync() gave up after ${elapsed}ms; expected it to spend at ` +
              `least ~${expected}ms on ${options.maxRetries} retries of ` +
              `${options.retryDelay}ms escalating delay`);

    // Catches unit confusion (e.g. seconds vs. milliseconds) in the delay.
    assert.ok(elapsed < common.platformTimeout(expected * 10),
              `rmSync() gave up after ${elapsed}ms; expected roughly ` +
              `${expected}ms for ${options.maxRetries} retries`);
  } finally {
    child.kill();
  }
  await once(child, 'exit');
  assert.strictEqual(fs.existsSync(file), true);
})().then(common.mustCall());
