'use strict';

const common = require('../common');
const stream = require('stream');
const assert = require('assert');

{
  const r = new stream.Readable({
    read: common.mustCall(function() {
      this.push('content');
      this.push(null);
    })
  });

  const t = new stream.Transform({
    transform: common.mustCall(function(chunk, encoding, callback) {
      this.push(chunk);
      return callback();
    }),
    flush: common.mustCall(function(callback) {
      return callback();
    })
  });

  r.pipe(t);
  t.on('readable', common.mustCall(function() {
    while (true) {
      const chunk = t.read();
      if (!chunk)
        break;

      assert.strictEqual(chunk.toString(), 'content');
    }
  }, 2));
}

{
  const t = new stream.Transform({
    transform: common.mustCall(function(chunk, encoding, callback) {
      this.push(chunk);
      return callback();
    }),
    flush: common.mustCall(function(callback) {
      return callback();
    })
  });

  t.end('content');

  t.on('readable', common.mustCall(function() {
    while (true) {
      const chunk = t.read();
      if (!chunk)
        break;
      assert.strictEqual(chunk.toString(), 'content');
    }
  }));
}

{
  const t = new stream.Transform({
    transform: common.mustCall(function(chunk, encoding, callback) {
      this.push(chunk);
      return callback();
    }),
    flush: common.mustCall(function(callback) {
      return callback();
    })
  });

  t.write('content');
  t.end();

  t.on('readable', common.mustCall(function() {
    while (true) {
      const chunk = t.read();
      if (!chunk)
        break;
      assert.strictEqual(chunk.toString(), 'content');
    }
  }));
}

{
  const t = new stream.Readable({
    read() {
    }
  });

  t.on('readable', common.mustCall(function() {
    while (true) {
      const chunk = t.read();
      if (!chunk)
        break;
      assert.strictEqual(chunk.toString(), 'content');
    }
  }));

  t.push('content');
  t.push(null);
}

{
  const t = new stream.Readable({
    read() {
    }
  });

  t.on('readable', common.mustCall(function() {
    while (true) {
      const chunk = t.read();
      if (!chunk)
        break;
      assert.strictEqual(chunk.toString(), 'content');
    }
  }, 2));

  process.nextTick(() => {
    t.push('content');
    t.push(null);
  });
}

{
  const t = new stream.Transform({
    transform: common.mustCall(function(chunk, encoding, callback) {
      this.push(chunk);
      return callback();
    }),
    flush: common.mustCall(function(callback) {
      return callback();
    })
  });

  t.on('readable', common.mustCall(function() {
    while (true) {
      const chunk = t.read();
      if (!chunk)
        break;
      assert.strictEqual(chunk.toString(), 'content');
    }
  }, 2));

  t.write('content');
  t.end();
}
