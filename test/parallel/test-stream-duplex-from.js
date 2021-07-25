'use strict';

const common = require('../common');
const assert = require('assert');
const { Duplex, Readable, Writable } = require('stream');

{
  const d = Duplex.from({
    readable: new Readable({
      read() {
        this.push('asd');
        this.push(null);
      }
    })
  });
  assert.strictEqual(d.readable, true);
  assert.strictEqual(d.writable, false);
  d.once('readable', common.mustCall(function() {
    assert.strictEqual(d.read().toString(), 'asd');
  }));
  d.once('end', common.mustCall(function() {
    assert.strictEqual(d.readable, false);
  }));
}

{
  const d = Duplex.from(new Readable({
    read() {
      this.push('asd');
      this.push(null);
    }
  }));
  assert.strictEqual(d.readable, true);
  assert.strictEqual(d.writable, false);
  d.once('readable', common.mustCall(function() {
    assert.strictEqual(d.read().toString(), 'asd');
  }));
  d.once('end', common.mustCall(function() {
    assert.strictEqual(d.readable, false);
  }));
}

{
  let ret = '';
  const d = Duplex.from(new Writable({
    write(chunk, encoding, callback) {
      ret += chunk;
      callback();
    }
  }));
  assert.strictEqual(d.readable, false);
  assert.strictEqual(d.writable, true);
  d.end('asd');
  d.on('finish', common.mustCall(function() {
    assert.strictEqual(d.writable, false);
    assert.strictEqual(ret, 'asd');
  }));
}

{
  let ret = '';
  const d = Duplex.from({
    writable: new Writable({
      write(chunk, encoding, callback) {
        ret += chunk;
        callback();
      }
    })
  });
  assert.strictEqual(d.readable, false);
  assert.strictEqual(d.writable, true);
  d.end('asd');
  d.on('finish', common.mustCall(function() {
    assert.strictEqual(d.writable, false);
    assert.strictEqual(ret, 'asd');
  }));
}

{
  let ret = '';
  const d = Duplex.from({
    readable: new Readable({
      read() {
        this.push('asd');
        this.push(null);
      }
    }),
    writable: new Writable({
      write(chunk, encoding, callback) {
        ret += chunk;
        callback();
      }
    })
  });
  assert.strictEqual(d.readable, true);
  assert.strictEqual(d.writable, true);
  d.once('readable', common.mustCall(function() {
    assert.strictEqual(d.read().toString(), 'asd');
  }));
  d.once('end', common.mustCall(function() {
    assert.strictEqual(d.readable, false);
  }));
  d.end('asd');
  d.once('finish', common.mustCall(function() {
    assert.strictEqual(d.writable, false);
    assert.strictEqual(ret, 'asd');
  }));
}

{
  const d = Duplex.from(Promise.resolve('asd'));
  assert.strictEqual(d.readable, true);
  assert.strictEqual(d.writable, false);
  d.once('readable', common.mustCall(function() {
    assert.strictEqual(d.read().toString(), 'asd');
  }));
  d.once('end', common.mustCall(function() {
    assert.strictEqual(d.readable, false);
  }));
}
