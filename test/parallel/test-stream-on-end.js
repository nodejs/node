'use strict';

const common = require('../common');
const { Writable, Readable, Transform, onEnd } = require('stream');
const assert = require('assert');
const fs = require('fs');
const { promisify } = require('util');

common.crashOnUnhandledRejection();

{
  const rs = new Readable({
    read() {}
  });

  onEnd(rs, common.mustCall((err) => {
    assert(!err, 'no error');
  }));

  rs.push(null);
  rs.resume();
}

{
  const ws = new Writable({
    write(data, enc, cb) {
      cb();
    }
  });

  onEnd(ws, common.mustCall((err) => {
    assert(!err, 'no error');
  }));

  ws.end();
}

{
  const tr = new Transform({
    transform(data, enc, cb) {
      cb();
    }
  });

  let finished = false;
  let ended = false;

  tr.on('end', () => {
    ended = true;
  });

  tr.on('finish', () => {
    finished = true;
  });

  onEnd(tr, common.mustCall((err) => {
    assert(!err, 'no error');
    assert(finished);
    assert(ended);
  }));

  tr.end();
  tr.resume();
}

{
  const rs = fs.createReadStream(__filename);

  rs.resume();
  onEnd(rs, common.mustCall());
}

{
  const finishedPromise = promisify(finished);

  async function run() {
    const rs = fs.createReadStream(__filename);
    const done = common.mustCall();

    let ended = false;
    rs.resume();
    rs.on('end', () => {
      ended = true;
    });
    await finishedPromise(rs);
    assert(ended);
    done();
  }

  run();
}

{
  const rs = fs.createReadStream('file-does-not-exist');

  onEnd(rs, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOENT');
  }));
}

{
  const rs = new Readable();

  onEnd(rs, common.mustCall((err) => {
    assert(!err, 'no error');
  }));

  rs.push(null);
  rs.emit('close'); // should not trigger an error
  rs.resume();
}

{
  const rs = new Readable();

  onEnd(rs, common.mustCall((err) => {
    assert(err, 'premature close error');
  }));

  rs.emit('close'); // should trigger error
  rs.push(null);
  rs.resume();
}
