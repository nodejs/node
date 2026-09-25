'use strict';
const common = require('../common');

if (common.isWindows) {
  // https://github.com/nodejs/node/issues/48300
  common.skip('Does not work with cygwin quirks on Windows');
}

const assert = require('assert');
const fs = require('fs');
const spawn = require('child_process').spawn;
const tmpdir = require('../common/tmpdir');

let cat, grep, wc;

const KB = 1024;
const MB = KB * KB;


// Make sure process chaining allows desired data flow:
// check cat <file> | grep 'x' | wc -c === 1MB
// This helps to make sure no data is lost between pipes.

{
  tmpdir.refresh();
  const file = tmpdir.resolve('data.txt');
  const buf = Buffer.alloc(MB).fill('x');

  // Most OS commands that deal with data, attach special meanings to new line -
  // for example, line buffering. So cut the buffer into lines at some points,
  // forcing data flow to be split in the stream. Do not use os.EOL for \n as
  // that is 2 characters on Windows and is sometimes converted to 1 character
  // which causes the test to fail.
  for (let i = 1; i < KB; i++)
    buf.write('\n', i * KB);
  fs.writeFileSync(file, buf.toString());

  cat = spawn('cat', [file]);
  grep = spawn('grep', ['x'], { stdio: [cat.stdout, 'pipe', 'pipe'] });
  wc = spawn('wc', ['-c'], { stdio: [grep.stdout, 'pipe', 'pipe'] });

  // Extra checks: We never try to start reading data ourselves.
  cat.stdout._handle.readStart = common.mustNotCall();
  grep.stdout._handle.readStart = common.mustNotCall();

  // Keep an array of error codes and assert on them during process exit. This
  // is because stdio can still be open when a child process exits, and we don't
  // want to lose information about what caused the error.
  const errors = [];
  process.on('exit', () => {
    assert.deepStrictEqual(errors, []);
  });

  [cat, grep, wc].forEach((child, index) => {
    const errorHandler = (thing, type) => {
      // Don't want to assert here, as we might miss error code info.
      console.error(`unexpected ${type} from child #${index}:\n${thing}`);
    };

    child.stderr.on('data', (d) => { errorHandler(d, 'data'); });
    child.on('error', (err) => { errorHandler(err, 'error'); });
    child.on('exit', common.mustCall((code) => {
      if (code !== 0) {
        errors.push(`child ${index} exited with code ${code}`);
      }
    }));
  });

  let wcBuf = '';
  wc.stdout.on('data', common.mustCall((data) => {
    wcBuf += data;
  }));

  process.on('exit', () => {
    // Grep always adds one extra byte at the end.
    assert.strictEqual(wcBuf.trim(), (MB + 1).toString());
  });
}
