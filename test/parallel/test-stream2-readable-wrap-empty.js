'use strict';
const common = require('../common');

const Readable = require('_stream_readable');
const EE = require('events').EventEmitter;

const oldStream = new EE();
oldStream.pause = () => {};
oldStream.resume = () => {};

const newStream = new Readable().wrap(oldStream);

newStream
  .on('readable', () => {})
  .on('end', common.mustCall());

oldStream.emit('end');
