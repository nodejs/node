'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

const replProcess = spawn(process.argv0, ['--interactive'], {
  stdio: ['pipe', 'pipe', 'inherit'],
  windowsHide: true,
});

replProcess.on('error', common.mustNotCall());

const replReadyState = (async function* () {
  let ready;
  const SPACE = ' '.charCodeAt();
  const BRACKET = '>'.charCodeAt();
  const DOT = '.'.charCodeAt();
  replProcess.stdout.on('data', (data) => {
    ready = data[data.length - 1] === SPACE && (
      data[data.length - 2] === BRACKET || (
        data[data.length - 2] === DOT &&
        data[data.length - 3] === DOT &&
        data[data.length - 4] === DOT
      ));
  });

  const processCrashed = new Promise((resolve, reject) =>
    replProcess.on('exit', reject)
  );
  while (true) {
    await Promise.race([new Promise(setImmediate), processCrashed]);
    if (ready) {
      ready = false;
      yield;
    }
  }
})();
async function writeLn(data, expectedOutput) {
  await replReadyState.next();
  if (expectedOutput) {
    replProcess.stdout.once('data', common.mustCall((data) =>
      assert.match(data.toString('utf8'), expectedOutput)
    ));
  }
  await new Promise((resolve, reject) => replProcess.stdin.write(
    `${data}\n`,
    (err) => (err ? reject(err) : resolve())
  ));
}

async function main() {
  await writeLn(
    'Object.defineProperty(Array.prototype, "-1", ' +
        '{ get() { return this[this.length - 1]; } });'
  );

  await writeLn(
    '[3, 2, 1][-1];',
    /^1\n(>\s)?$/
  );
  await writeLn('.exit');

  assert(!replProcess.connected);
}

main().then(common.mustCall());
