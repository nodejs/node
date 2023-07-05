'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');

const rs = new Readable({
  read() {}
});

let closed = false;
let errored = false;

rs.on('close', common.mustCall(() => {
  closed = true;
  assert(errored);
}));

rs.on('error', common.mustCall((err) => {
  errored = true;
  assert(!closed);
}));

rs.destroy(new Error('kaboom'));
