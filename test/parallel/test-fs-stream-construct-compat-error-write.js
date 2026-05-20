'use strict';

const common = require('../common');
const fs = require('fs');
const assert = require('assert');

const debuglog = (arg) => {
  console.log(new Date().toLocaleString(), arg);
};

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

{
  // Compat error.
  debuglog('start test');

  function WriteStream(...args) {
    debuglog('WriteStream constructor');
    fs.WriteStream.call(this, ...args);
  }
  Object.setPrototypeOf(WriteStream.prototype, fs.WriteStream.prototype);
  Object.setPrototypeOf(WriteStream, fs.WriteStream);

  WriteStream.prototype.open = common.mustCall(function WriteStream$open() {
    debuglog('WriteStream open() callback');
    const that = this;
    fs.open(that.path, that.flags, that.mode, (err, fd) => {
      debuglog('inner fs open() callback');
      that.emit('error', err);
    });
  });

  fs.open(`${tmpdir.path}/dummy`, 'wx+', common.mustCall((err, fd) => {
    debuglog('fs open() callback');
    assert.ifError(err);
    fs.close(fd, () => { debuglog(`closed ${fd}`); });
    const w = new WriteStream(`${tmpdir.path}/dummy`,
                              { flags: 'wx+', emitClose: true })
      .on('error', common.mustCall((err) => {
        debuglog('error event callback');
        assert.strictEqual(err.code, 'EEXIST');
        w.destroy();
        w.on('close', common.mustCall(() => {
          debuglog('close event callback');
        }));
      }));
  }));
  debuglog('waiting for callbacks');
}
