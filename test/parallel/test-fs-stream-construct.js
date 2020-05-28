'use strict';

const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const examplePath = fixtures.path('x.txt');

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
  const r = new ReadStream(examplePath)
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

{
  // Compat with graceful-fs.

  function ReadStream(...args) {
    fs.ReadStream.call(this, ...args);
  }
  Object.setPrototypeOf(ReadStream.prototype, fs.ReadStream.prototype);
  Object.setPrototypeOf(ReadStream, fs.ReadStream);

  ReadStream.prototype.open = common.mustCall(function ReadStream$open() {
    const that = this;
    fs.open(that.path, that.flags, that.mode, (err, fd) => {
      if (err) {
        if (that.autoClose)
          that.destroy();

        that.emit('error', err);
      } else {
        that.fd = fd;
        that.emit('open', fd);
        that.read();
      }
    });
  });

  const r = new ReadStream(examplePath)
    .on('open', common.mustCall((fd) => {
      assert.strictEqual(fd, r.fd);
      r.destroy();
    }));
}

{
  // Compat with graceful-fs.

  function WriteStream(...args) {
    fs.WriteStream.call(this, ...args);
  }
  Object.setPrototypeOf(WriteStream.prototype, fs.WriteStream.prototype);
  Object.setPrototypeOf(WriteStream, fs.WriteStream);

  WriteStream.prototype.open = common.mustCall(function WriteStream$open() {
    const that = this;
    fs.open(that.path, that.flags, that.mode, function(err, fd) {
      if (err) {
        that.destroy();
        that.emit('error', err);
      } else {
        that.fd = fd;
        that.emit('open', fd);
      }
    });
  });

  const w = new WriteStream(`${tmpdir.path}/dummy2`)
    .on('open', common.mustCall((fd) => {
      assert.strictEqual(fd, w.fd);
      w.destroy();
    }));
}

{
  // Compat error.

  function ReadStream(...args) {
    fs.ReadStream.call(this, ...args);
  }
  Object.setPrototypeOf(ReadStream.prototype, fs.ReadStream.prototype);
  Object.setPrototypeOf(ReadStream, fs.ReadStream);

  ReadStream.prototype.open = common.mustCall(function ReadStream$open() {
    const that = this;
    fs.open(that.path, that.flags, that.mode, (err, fd) => {
      that.emit('error', err);
    });
  });

  const r = new ReadStream('/doesnotexist', { emitClose: true })
    .on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ENOENT');
      assert.strictEqual(r.destroyed, true);
      r.on('close', common.mustCall());
    }));
}

{
  // Compat error.

  function WriteStream(...args) {
    fs.WriteStream.call(this, ...args);
  }
  Object.setPrototypeOf(WriteStream.prototype, fs.WriteStream.prototype);
  Object.setPrototypeOf(WriteStream, fs.WriteStream);

  WriteStream.prototype.open = common.mustCall(function WriteStream$open() {
    const that = this;
    fs.open(that.path, that.flags, that.mode, (err, fd) => {
      that.emit('error', err);
    });
  });

  fs.open(`${tmpdir.path}/dummy3`, 'wx+', common.mustCall((err) => {
    assert(!err);
    const w = new WriteStream(`${tmpdir.path}/dummy3`,
                              { flags: 'wx+', emitClose: true })
      .on('error', common.mustCall((err) => {
        assert.strictEqual(err.code, 'EEXIST');
        w.destroy();
        w.on('close', common.mustCall());
      }));
  }));
}
