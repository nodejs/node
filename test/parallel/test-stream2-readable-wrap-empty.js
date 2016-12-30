'use strict';
const common = require('../common');

const Readable = require('_stream_readable');
const EE = require('events').EventEmitter;

var oldStream = new EE();
oldStream.pause = function() {};
oldStream.resume = function() {};

var newStream = new Readable().wrap(oldStream);

newStream
  .on('readable', function() {})
  .on('end', common.mustCall(function() {}));

oldStream.emit('end');
