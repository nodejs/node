'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');

tmpdir.refresh();

const data = 'AAAABBBBCCCC';

// Report back fewer bytes than were handed over, asynchronously: the retry has
// to observe the position as it is after _writev() advanced this.pos by the
// whole batch.
function shortWrite(bytes) {
  return (fd, chunks, position, cb) => {
    const head = Buffer.concat(chunks).subarray(0, bytes);
    fs.write(fd, head, 0, bytes, position,
             (er, bytesWritten) => cb(er, bytesWritten, chunks));
  };
}

function eagain(fd, chunks, position, cb) {
  process.nextTick(cb, Object.assign(new Error(), { code: 'EAGAIN' }), 0, chunks);
}

// The positions handed to writev() are the contract under test: watching the
// resulting file alone would still pass if some later rewrite happened to land
// the bytes correctly by another route.
function run(name, { prefix = '', start, firstCall }, expectedPositions) {
  const file = tmpdir.resolve(`partial-writev-${name}.txt`);
  fs.writeFileSync(file, prefix);

  const positions = [];
  const stream = fs.createWriteStream(file, {
    flags: 'r+',
    start,
    fs: {
      ...fs,
      writev: common.mustCall((fd, chunks, position, cb) => {
        positions.push(position);
        if (positions.length === 1) {
          firstCall(fd, chunks, position, cb);
        } else {
          fs.writev(fd, chunks, position, cb);
        }
      }, 2),
    },
  });

  stream.cork();
  stream.write('AAAA');
  stream.write('BBBB');
  stream.write('CCCC');
  stream.uncork();
  stream.end();

  stream.on('error', common.mustNotCall());
  stream.on('close', common.mustCall(() => {
    if (expectedPositions !== undefined) {
      assert.deepStrictEqual(positions, expectedPositions, `${name} positions`);
    }
    assert.strictEqual(fs.readFileSync(file, 'utf8'), prefix + data, name);
  }));
}

// Partial write: resume right after the bytes that reached the disk.
run('start-5', { prefix: '#####', start: 5, firstCall: shortWrite(3) }, [5, 8]);
run('start-0', { start: 0, firstCall: shortWrite(3) }, [0, 3]);

// EAGAIN: nothing was written, so the retry goes back to the same position.
run('eagain', { prefix: '#####', start: 5, firstCall: eagain }, [5, 5]);

// Without a start the stream follows the fd offset; only the bytes matter.
run('no-start', { firstCall: shortWrite(3) });
