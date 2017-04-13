'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const tty = require('tty');


assert.throws(() => {
  new tty.WriteStream(-1);
});

assert.throws(() => {
  let fd = 2;
  // Get first known bad file descriptor.
  try {
    while (fs.fstatSync(++fd));
  } catch (e) { }
  new tty.WriteStream(fd);
});

assert.throws(() => {
  new tty.ReadStream(-1);
});

assert.throws(() => {
  let fd = 2;
  // Get first known bad file descriptor.
  try {
    while (fs.fstatSync(++fd));
  } catch (e) { }
  new tty.ReadStream(fd);
});
