'use strict';
require('../common');
const assert = require('assert');

const stream = require('stream');
const Readable = stream.Readable;
const Writable = stream.Writable;

const totalChunks = 100;
const chunkSize = 99;
const expectTotalData = totalChunks * chunkSize;
let expectEndingData = expectTotalData;

const r = new Readable({ highWaterMark: 1000 });
let chunks = totalChunks;
r._read = function(n) {
  if (!(chunks % 2))
    setImmediate(push);
  else if (!(chunks % 3))
    process.nextTick(push);
  else
    push();
};

let totalPushed = 0;
function push() {
  const chunk = chunks-- > 0 ? Buffer.alloc(chunkSize, 'x') : null;
  if (chunk) {
    totalPushed += chunk.length;
  }
  r.push(chunk);
}

read100();

// first we read 100 bytes
function read100() {
  readn(100, onData);
}

function readn(n, then) {
  console.error(`read ${n}`);
  expectEndingData -= n;
  (function read() {
    const c = r.read(n);
    if (!c)
      r.once('readable', read);
    else {
      assert.strictEqual(c.length, n);
      assert(!r._readableState.flowing);
      then();
    }
  })();
}

// then we listen to some data events
function onData() {
  expectEndingData -= 100;
  console.error('onData');
  let seen = 0;
  r.on('data', function od(c) {
    seen += c.length;
    if (seen >= 100) {
      // seen enough
      r.removeListener('data', od);
      r.pause();
      if (seen > 100) {
        // oh no, seen too much!
        // put the extra back.
        const diff = seen - 100;
        r.unshift(c.slice(c.length - diff));
        console.error('seen too much', seen, diff);
      }

      // Nothing should be lost in between
      setImmediate(pipeLittle);
    }
  });
}

// Just pipe 200 bytes, then unshift the extra and unpipe
function pipeLittle() {
  expectEndingData -= 200;
  console.error('pipe a little');
  const w = new Writable();
  let written = 0;
  w.on('finish', function() {
    assert.strictEqual(written, 200);
    setImmediate(read1234);
  });
  w._write = function(chunk, encoding, cb) {
    written += chunk.length;
    if (written >= 200) {
      r.unpipe(w);
      w.end();
      cb();
      if (written > 200) {
        const diff = written - 200;
        written -= diff;
        r.unshift(chunk.slice(chunk.length - diff));
      }
    } else {
      setImmediate(cb);
    }
  };
  r.pipe(w);
}

// now read 1234 more bytes
function read1234() {
  readn(1234, resumePause);
}

function resumePause() {
  console.error('resumePause');
  // don't read anything, just resume and re-pause a whole bunch
  r.resume();
  r.pause();
  r.resume();
  r.pause();
  r.resume();
  r.pause();
  r.resume();
  r.pause();
  r.resume();
  r.pause();
  setImmediate(pipe);
}


function pipe() {
  console.error('pipe the rest');
  const w = new Writable();
  let written = 0;
  w._write = function(chunk, encoding, cb) {
    written += chunk.length;
    cb();
  };
  w.on('finish', function() {
    console.error('written', written, totalPushed);
    assert.strictEqual(written, expectEndingData);
    assert.strictEqual(totalPushed, expectTotalData);
    console.log('ok');
  });
  r.pipe(w);
}
