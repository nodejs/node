'use strict';
require('../common');

const TTY = process.binding('tty_wrap').TTY;
const WriteWrap = process.binding('stream_wrap').WriteWrap;

const handle = new TTY(1);
const req = new WriteWrap();

handle.writeBuffer(req, Buffer.from('hello world 1\n'));
handle.writeBuffer(req, Buffer.from('hello world 2\n'));
