'use strict';
const common = require('../common');
const assert = require('assert');

const { Writable } = require('stream');

function expectError(w, arg, code, sync) {
  if (sync) {
    if (code) {
      assert.throws(() => w.write(arg), { code });
    } else {
      w.write(arg);
    }
  } else {
    let errorCalled = false;
    let ticked = false;
    w.write(arg, common.mustCall((err) => {
      assert.strictEqual(ticked, true);
      assert.strictEqual(errorCalled, false);
      assert.strictEqual(err.code, code);
    }));
    ticked = true;
    w.on('error', common.mustCall((err) => {
      errorCalled = true;
      assert.strictEqual(err.code, code);
    }));
  }
}

function test(autoDestroy) {
  {
    const w = new Writable({
      autoDestroy,
      _write() {}
    });
    w.end();
    expectError(w, 'asd', 'ERR_STREAM_WRITE_AFTER_END');
  }

  {
    const w = new Writable({
      autoDestroy,
      _write() {}
    });
    w.destroy();
  }

  {
    const w = new Writable({
      autoDestroy,
      _write() {}
    });
    expectError(w, null, 'ERR_STREAM_NULL_VALUES', true);
  }

  {
    const w = new Writable({
      autoDestroy,
      _write() {}
    });
    expectError(w, {}, 'ERR_INVALID_ARG_TYPE', true);
  }
}

test(false);
test(true);
