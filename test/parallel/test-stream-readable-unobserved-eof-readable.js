'use strict';
const common = require('../common');
const { Readable } = require('stream');
const assert = require('assert');

// When a stream reaches its end while nobody is observing it, the
// end-of-stream 'readable' emission is not scheduled. It is emitted
// later if a 'readable' listener is attached, and read()/resume()
// still reach 'end' on their own.

{
  // Listener attached synchronously after push(null) gets 'readable'
  // and then 'end'.
  const r = new Readable({ read() {} });
  r.push(null);
  let readableEmitted = false;
  r.on('readable', common.mustCall(() => {
    readableEmitted = true;
    assert.strictEqual(r.read(), null);
  }));
  r.on('end', common.mustCall(() => {
    assert.strictEqual(readableEmitted, true);
  }));
}

{
  // Listener attached one macrotask after the unobserved end still gets
  // the owed 'readable' before 'end'.
  const r = new Readable({ read() {} });
  r.push(null);
  setImmediate(common.mustCall(() => {
    let readableEmitted = false;
    r.on('readable', common.mustCall(() => {
      readableEmitted = true;
      assert.strictEqual(r.read(), null);
    }));
    r.on('end', common.mustCall(() => {
      assert.strictEqual(readableEmitted, true);
    }));
  }));
}

{
  // A stream that ends unobserved still emits 'end' when resumed later.
  const r = new Readable({ read() {} });
  r.push(null);
  setImmediate(common.mustCall(() => {
    r.resume();
    r.on('end', common.mustCall());
  }));
}

{
  // read() after an unobserved end consumes the owed notification: a
  // 'readable' listener attached afterwards does not receive it, but
  // 'end' is still emitted.
  const r = new Readable({ read() {} });
  r.push(null);
  setImmediate(common.mustCall(() => {
    assert.strictEqual(r.read(), null);
    r.on('end', common.mustCall());
  }));
}

{
  // A 'data' listener attached after an unobserved end still gets 'end'.
  const r = new Readable({ read() {} });
  r.push('x');
  r.push(null);
  setImmediate(common.mustCall(() => {
    r.on('data', common.mustCall((chunk) => {
      assert.strictEqual(chunk.toString(), 'x');
    }));
    r.on('end', common.mustCall());
  }));
}
