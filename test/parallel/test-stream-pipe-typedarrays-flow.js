'use strict';
const common = require('../common');
const { Readable, Writable, PassThrough } = require('stream');

// getArrayBufferViews requires buffer length to be multiple of 8
const msgBuffer = Buffer.from('hello'.repeat(8));
const testBuffers = common.getArrayBufferViews(msgBuffer);

{
  let ticks = testBuffers.length;

  const rs = new Readable({
    objectMode: false,
    read: () => {
      if (ticks > 0) {
        process.nextTick(() => rs.push(testBuffers[ticks - 1]));
        ticks--;
        return;
      }
      rs.push(Buffer.from(''));
      rs.push(null);
    }
  });

  const ws = new Writable({
    highWaterMark: 0,
    objectMode: false,
    write: (data, end, cb) => setImmediate(cb)
  });

  rs.on('end', common.mustCall());
  ws.on('finish', common.mustCall());
  rs.pipe(ws);
}

{
  let missing = 8;

  const rs = new Readable({
    objectMode: false,
    read: () => {
      if (missing--) rs.push(testBuffers[missing]);
      else rs.push(null);
    }
  });

  const pt = rs
    .pipe(new PassThrough({ objectMode: false, highWaterMark: 2 }))
    .pipe(new PassThrough({ objectMode: false, highWaterMark: 2 }));

  pt.on('end', function() {
    wrapper.push(null);
  });

  const wrapper = new Readable({
    objectMode: false,
    read: () => {
      process.nextTick(function() {
        let data = pt.read();
        if (data === null) {
          pt.once('readable', function() {
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
