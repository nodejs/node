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
  // Ensure unpipe is only called once.
  function testUnpipeOnce(fn) {
    const r = new Readable({
      read: () => {}
    });
    const w = new PassThrough();
    r.unpipe = common.mustCall();
    r.pipe(w);
    w.on('error', () => {});
    fn(w);
  }

  function permute(arr) {
    return arr.reduce((res, item, key, arr) => {
      return res
        .concat(arr.length > 1 && arr.slice(0, key)
          .concat(arr.slice(key + 1))
          .reduce(permute, []).map((perm) => {
            return [item].concat(perm);
          }) || item);
    }, []);
  }
  for (const emits of permute([ 'error', 'finish', 'close' ])) {
    testUnpipeOnce((w) => {
      for (const emit of emits) {
        w.emit(emit);
      }
    });
  }
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
