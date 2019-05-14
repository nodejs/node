// Flags: --expose-internals
'use strict';
const common = require('../common');

const noop = () => {};
const { internalBinding } = require('internal/test/binding');
const TTY = internalBinding('tty_wrap').TTY = function() {};

TTY.prototype = {
  setBlocking: noop,
  getWindowSize: noop
};

const { WriteStream } = require('tty');

const methods = [
  'cursorTo',
  'moveCursor',
  'clearLine',
  'clearScreenDown'
];

methods.forEach((method) => {
  require('readline')[method] = common.mustCall();
  const writeStream = new WriteStream(1);
  writeStream[method](1, 2);
});
