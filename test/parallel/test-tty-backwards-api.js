'use strict';
const common = require('../common');

const noop = () => {};
const TTY = process.binding('tty_wrap').TTY = function() {};

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
