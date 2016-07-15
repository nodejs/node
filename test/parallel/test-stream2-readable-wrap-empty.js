'use strict';
const common = require('../common');

var Readable = require('_stream_readable');
var EE = require('events').EventEmitter;

var oldStream = new EE();
oldStream.pause = function() {};
oldStream.resume = function() {};

var newStream = new Readable().wrap(oldStream);

newStream
  .on('readable', function() {})
  .on('end', common.mustCall(function() {}));

oldStream.emit('end');
