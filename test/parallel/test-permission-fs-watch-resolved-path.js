'use strict';

// fs.watch() resolves the watched path once, when the watch starts. The read
// scope is checked against that canonical path, and so are the entries that
// are reported afterwards.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('no crypto');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// Runs `script` in a child with the permission model enabled. The script
// prints `READY` when the parent has to trigger the change, and prints a
// final line starting with `RESULT:`.
function run({ grants, script, cwd, trigger }) {
  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, [
      '--permission',
      ...grants,
      '-e',
      script,
    ], { encoding: 'utf8', cwd });

    let stdout = '';
    let stderr = '';
    let triggered = false;
    child.stdout.setEncoding('utf8');
    child.stderr.setEncoding('utf8');
    child.stderr.on('data', (chunk) => { stderr += chunk; });
    child.stdout.on('data', (chunk) => {
      stdout += chunk;
      if (!triggered && trigger && stdout.includes('READY\n')) {
        triggered = true;
        trigger();
      }
    });
    child.on('close', (code) => {
      if (code !== 0) return reject(new Error(`${stderr}\n${stdout}`));
      const line = stdout.split('\n').find((l) => l.startsWith('RESULT:'));
      if (line === undefined) return reject(new Error(stdout));
      resolve(JSON.parse(line.slice('RESULT:'.length)));
    });
  });
}

// The names of the entries are checked against the watched directory, even if
// the working directory changes after the watch started. `watched` does not
// exist when the permissions are applied, so its grant is exact and the
// entries inside it are not readable.
async function testChdir() {
  const base = tmpdir.resolve('cwd');
  const watched = path.join(base, 'watched');
  const other = path.join(base, 'other');
  fs.mkdirSync(other, { recursive: true });

  const result = await run({
    cwd: base,
    grants: [
      `--allow-fs-read=${watched}`,
      `--allow-fs-write=${watched}`,
      `--allow-fs-read=${other}`,
    ],
    script: `
      const fs = require('fs');
      fs.mkdirSync('watched');
      const w = fs.watch('watched', (event, filename) => {
        w.close();
        process.stdout.write('RESULT:' + JSON.stringify(filename) + '\\n');
      });
      process.chdir(${JSON.stringify(other)});
      process.stdout.write('READY\\n');
    `,
    trigger() {
      fs.writeFileSync(path.join(watched, 'file.txt'), 'x');
    },
  });
  assert.strictEqual(result, null);
}

// Watching a single file works, and reports the name of the file.
async function testFile() {
  const base = tmpdir.resolve('file');
  const file = path.join(base, 'file.txt');
  fs.mkdirSync(base);
  fs.writeFileSync(file, 'x');

  const result = await run({
    grants: [`--allow-fs-read=${file}`],
    script: `
      const fs = require('fs');
      const w = fs.watch(${JSON.stringify(file)}, (event, filename) => {
        w.close();
        process.stdout.write('RESULT:' + JSON.stringify(filename) + '\\n');
      });
      process.stdout.write('READY\\n');
    `,
    trigger() {
      fs.writeFileSync(file, 'y');
    },
  });
  assert.strictEqual(result, 'file.txt');
}

// A symbolic link in a granted directory that points outside of it is denied
// when the watch starts, both for fs.watch() and fs.promises.watch().
async function testOutsideLink() {
  const base = tmpdir.resolve('link');
  const granted = path.join(base, 'granted');
  const outside = path.join(base, 'outside');
  const link = path.join(granted, 'link');
  fs.mkdirSync(granted, { recursive: true });
  fs.mkdirSync(outside);
  // A junction is used so that this also works on Windows.
  fs.symlinkSync(outside, link, 'junction');

  const result = await run({
    grants: [`--allow-fs-read=${granted}`],
    script: `
      const fs = require('fs');
      const codes = [];
      try {
        fs.watch(${JSON.stringify(link)}).close();
      } catch (e) {
        codes.push(e.code);
      }
      (async () => {
        const ac = new AbortController();
        const iterator = fs.promises.watch(${JSON.stringify(link)}, {
          signal: ac.signal,
        });
        const next = iterator.next();
        ac.abort();
        try {
          await next;
        } catch (e) {
          codes.push(e.code);
        }
        process.stdout.write('RESULT:' + JSON.stringify(codes) + '\\n');
      })();
    `,
  });
  assert.deepStrictEqual(result, ['ERR_ACCESS_DENIED', 'ERR_ACCESS_DENIED']);
}

// A symbolic link that resolves inside the granted scope is accepted.
async function testInsideLink() {
  const base = tmpdir.resolve('inside');
  const target = path.join(base, 'target');
  const link = path.join(base, 'link');
  fs.mkdirSync(target, { recursive: true });
  fs.symlinkSync(target, link, 'junction');

  const result = await run({
    grants: [`--allow-fs-read=${base}`],
    script: `
      const fs = require('fs');
      const w = fs.watch(${JSON.stringify(link)}, (event, filename) => {
        w.close();
        process.stdout.write('RESULT:' + JSON.stringify(filename) + '\\n');
      });
      process.stdout.write('READY\\n');
    `,
    trigger() {
      fs.writeFileSync(path.join(target, 'file.txt'), 'x');
    },
  });
  assert.strictEqual(result, 'file.txt');
}

// A granted path that goes through a symbolic link chosen by the user is not
// followed outside of the granted scope.
async function testUserAliasDenied() {
  const base = tmpdir.resolve('alias');
  const real = path.join(base, 'real');
  const alias = path.join(base, 'alias');
  const granted = path.join(alias, 'dir');
  fs.mkdirSync(path.join(real, 'dir'), { recursive: true });
  fs.symlinkSync(real, alias, 'junction');

  const result = await run({
    grants: [`--allow-fs-read=${granted}`],
    script: `
      const fs = require('fs');
      try {
        fs.watch(${JSON.stringify(granted)}).close();
        process.stdout.write('RESULT:null\\n');
      } catch (e) {
        process.stdout.write('RESULT:' + JSON.stringify(e.code) + '\\n');
      }
    `,
  });
  assert.strictEqual(result, 'ERR_ACCESS_DENIED');
}

// A `..` that follows a symbolic link is resolved by the operating system, so
// the path must be checked after the link is followed.
async function testDotDotThroughLink() {
  const base = tmpdir.resolve('dotdot');
  const granted = path.join(base, 'granted');
  const outside = path.join(base, 'outside');
  const link = path.join(granted, 'link');
  fs.mkdirSync(granted, { recursive: true });
  fs.mkdirSync(path.join(outside, 'sub'), { recursive: true });
  fs.mkdirSync(path.join(outside, 'y'));
  fs.mkdirSync(path.join(granted, 'y'));
  fs.symlinkSync(path.join(outside, 'sub'), link, 'junction');

  const result = await run({
    grants: [`--allow-fs-read=${granted}`],
    script: `
      const fs = require('fs');
      try {
        fs.watch(${JSON.stringify(link)} + '/../y').close();
        process.stdout.write('RESULT:null\\n');
      } catch (e) {
        process.stdout.write('RESULT:' + JSON.stringify(e.code) + '\\n');
      }
    `,
  });
  assert.strictEqual(result, 'ERR_ACCESS_DENIED');
}

(async () => {
  await testChdir();
  await testFile();
  await testOutsideLink();
  await testInsideLink();
  await testUserAliasDenied();
  await testDotDotThroughLink();
})().then(common.mustCall());
