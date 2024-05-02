'use strict';
const common = require('../common');
const assert = require('assert');
const { Readable, Writable, PassThrough } = require('stream');

{
  let ticks = 17;

  const rs = new Readable({
    objectMode: true,
    read: () => {
      if (ticks-- > 0)
        return process.nextTick(() => rs.push({}));
      rs.push({});
      rs.push(null);
    }
  });

  const ws = new Writable({
    highWaterMark: 0,
    objectMode: true,
    write: (data, end, cb) => setImmediate(cb)
  });

  rs.on('end', common.mustCall());
  ws.on('finish', common.mustCall());
  rs.pipe(ws);
}

{
  let missing = 8;

  const rs = new Readable({
    objectMode: true,
    read: () => {
      if (missing--) rs.push({});
      else rs.push(null);
    }
  });

  const pt = rs
    .pipe(new PassThrough({ objectMode: true, highWaterMark: 2 }))
    .pipe(new PassThrough({ objectMode: true, highWaterMark: 2 }));

  pt.on('end', () => {
    wrapper.push(null);
  });

  const wrapper = new Readable({
    objectMode: true,
    read: () => {
      process.nextTick(() => {
        let data = pt.read();
        if (data === null) {
          pt.once('readable', () => {
            data = pt.read();
            if (data !== null) wrapper.push(data);
          });
        } else {
          wrapper.push(data);
        }
      });
    }
  });

  wrapper.resume();
  wrapper.on('end', common.mustCall());
}

{
  // Only register drain if there is backpressure.
  const rs = new Readable({ read() {} });

  const pt = rs
    .pipe(new PassThrough({ objectMode: true, highWaterMark: 2 }));
  assert.strictEqual(pt.listenerCount('drain'), 0);
  pt.on('finish', () => {
    assert.strictEqual(pt.listenerCount('drain'), 0);
  });

  rs.push('asd');
  assert.strictEqual(pt.listenerCount('drain'), 0);

  process.nextTick(() => {
    rs.push('asd');
    assert.strictEqual(pt.listenerCount('drain'), 0);
    rs.push(null);
    assert.strictEqual(pt.listenerCount('drain'), 0);
  });
}
