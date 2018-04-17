'use strict';

const common = require('../common');
const { Duplex } = require('stream');
const { strictEqual } = require('assert');

const duplex = new Duplex({
  write(chunk, enc, cb) {
    cb(new Error('kaboom'));
  },
  read() {
    this.push(null);
  }
});

duplex.on('error', common.mustCall(function() {
  strictEqual(this._readableState.errorEmitted, true);
  strictEqual(this._writableState.errorEmitted, true);
}));

duplex.on('end', common.mustNotCall());

duplex.end('hello');
duplex.resume();
