'use strict';
const common = require('../common');

const Readable = require('_stream_readable');
const EE = require('events').EventEmitter;

const oldStream = new EE();
oldStream.pause = function() {};
oldStream.resume = function() {};

const newStream = new Readable().wrap(oldStream);

newStream
  .on('readable', function() {})
  .on('end', common.mustCall(function() {}));

oldStream.emit('end');
