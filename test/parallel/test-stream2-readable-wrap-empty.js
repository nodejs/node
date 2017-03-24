'use strict';
const common = require('../common');

const Readable = require('_stream_readable');
const EE = require('events').EventEmitter;

const oldStream = new EE();
oldStream.pause = common.noop;
oldStream.resume = common.noop;

const newStream = new Readable().wrap(oldStream);

newStream
  .on('readable', common.noop)
  .on('end', common.mustCall());

oldStream.emit('end');
