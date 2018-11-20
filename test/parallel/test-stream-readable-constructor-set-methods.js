'use strict';
const common = require('../common');

const Readable = require('stream').Readable;

const _read = common.mustCall(function _read(n) {
  this.push(null);
});

const r = new Readable({ read: _read });
r.resume();
