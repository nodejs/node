'use strict';

const stream = require('stream');
require('../common');
const assert = require('assert');
const crypto = require('crypto');
const pump = stream.pump;
// tiny node-tap lookalike.
const tests = [];
let count = 0;

function test(name, fn) {
  count++;
  tests.push([name, fn]);
}

function run() {
  const next = tests.shift();
  if (!next)
    return console.error('ok');

  const name = next[0];
  const fn = next[1];
  console.log('# %s', name);
  fn({
    same: assert.deepStrictEqual,
    equal: assert.strictEqual,
    end: function() {
      count--;
      run();
    }
  });
}

// ensure all tests have run
process.on('exit', function() {
  assert.strictEqual(count, 0);
});

process.nextTick(run);

test('basic pump', (t) => {
  const rs = new stream.Readable({
    read(size) {
      this.push(crypto.randomBytes(size));
    }
  });
  const ws = new stream.Writable({
    write(chunk, enc, next) {
      setImmediate(next);
    }
  });

  function toHex() {
    const reverse = new stream.Transform();

    reverse._transform = function(chunk, enc, callback) {
      reverse.push(chunk.toString('hex'));
      callback();
    };

    return reverse;
  }

  let wsClosed = false;
  let rsClosed = false;
  let callbackCalled = false;
  const timeout = setTimeout(function() {
    throw new Error('timeout');
  }, 5000);
  function check() {
    if (wsClosed && rsClosed && callbackCalled) {
      clearTimeout(timeout);
      t.end();
    }
  }

  ws.on('finish', function() {
    wsClosed = true;
    check();
  });

  rs.on('end', function() {
    rsClosed = true;
    check();
  });

  pump(rs, toHex(), toHex(), toHex(), ws, function() {
    callbackCalled = true;
    check();
  });

  setTimeout(function() {
    rs.push(null);
  }, 1000);
});
test('call destroy if error', (t) => {
  let wsDestroyCalled = false;
  let rsDestroyCalled = false;
  let callbackCalled = false;
  const rs = new stream.Readable({
    read(size) {
      this.push(crypto.randomBytes(size));
    }
  });
  rs.destroy = function() {
    this.emit('close');
    rsDestroyCalled = true;
    check();
  };
  const ws = new stream.Writable({
    write(chunk, enc, next) {
      setImmediate(next);
    }
  });
  ws.destroy = function() {
    this.emit('close');
    wsDestroyCalled = true;
    check();
  };
  function throwError() {
    const reverse = new stream.Transform();
    let i = 0;
    reverse._transform = function(chunk, enc, callback) {
      i++;
      if (i > 5) {
        return callback(new Error('lets end this'));
      }
      reverse.push(chunk.toString('hex'));
      callback();
    };

    return reverse;
  }
  function toHex() {
    const reverse = new stream.Transform();

    reverse._transform = function(chunk, enc, callback) {
      reverse.push(chunk.toString('hex'));
      callback();
    };

    return reverse;
  }

  function check() {
    if (wsDestroyCalled && rsDestroyCalled && callbackCalled) {
      t.end();
    }
  }

  pump(rs, toHex(), throwError(), toHex(), toHex(), ws, function(err) {
    t.equal(err && err.message, 'lets end this');
    callbackCalled = true;
    check();
  });
});
