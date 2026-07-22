'use strict';

const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

{
  // Compat with old node.

  function ReadStream(...args) {
    fs.ReadStream.call(this, ...args);
  }
  Object.setPrototypeOf(ReadStream.prototype, fs.ReadStream.prototype);
  Object.setPrototypeOf(ReadStream, fs.ReadStream);

  ReadStream.prototype.open = common.mustCall(function() {
    fs.open(this.path, this.flags, this.mode, (er, fd) => {
      if (er) {
        if (this.autoClose) {
          this.destroy();
        }
        this.emit('error', er);
        return;
      }

      this.fd = fd;
      this.emit('open', fd);
      this.emit('ready');
    });
  });

  let readyCalled = false;
  let ticked = false;
  const r = new ReadStream(fixtures.path('x.txt'))
    .on('ready', common.mustCall(() => {
      readyCalled = true;
      // Make sure 'ready' is emitted in same tick as 'open'.
      assert.strictEqual(ticked, false);
    }))
    .on('error', common.mustNotCall())
    .on('open', common.mustCall((fd) => {
      process.nextTick(() => {
        ticked = true;
        r.destroy();
      });
      assert.strictEqual(readyCalled, false);
      assert.strictEqual(fd, r.fd);
    }));
}

{
  // Compat with old node.

  function WriteStream(...args) {
    fs.WriteStream.call(this, ...args);
  }
  Object.setPrototypeOf(WriteStream.prototype, fs.WriteStream.prototype);
  Object.setPrototypeOf(WriteStream, fs.WriteStream);

  WriteStream.prototype.open = common.mustCall(function() {
    fs.open(this.path, this.flags, this.mode, (er, fd) => {
      if (er) {
        if (this.autoClose) {
          this.destroy();
        }
        this.emit('error', er);
        return;
      }

      this.fd = fd;
      this.emit('open', fd);
      this.emit('ready');
    });
  });

  let readyCalled = false;
  let ticked = false;
  const w = new WriteStream(`${tmpdir.path}/dummy`)
    .on('ready', common.mustCall(() => {
      readyCalled = true;
      // Make sure 'ready' is emitted in same tick as 'open'.
      assert.strictEqual(ticked, false);
    }))
    .on('error', common.mustNotCall())
    .on('open', common.mustCall((fd) => {
      process.nextTick(() => {
        ticked = true;
        w.destroy();
      });
      assert.strictEqual(readyCalled, false);
      assert.strictEqual(fd, w.fd);
    }));
}
