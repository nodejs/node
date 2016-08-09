'use strict';

const EE = require('events');
const util = require('util');

function Stream() {
  EE.call(this);
}
util.inherits(Stream, EE);

// wrap the old-style stream
require('_stream_legacy')(Stream);

Stream.Readable = require('_stream_readable');
Stream.Writable = require('_stream_writable');
Stream.Duplex = require('_stream_duplex');
Stream.Transform = require('_stream_transform');
Stream.PassThrough = require('_stream_passthrough');

// Backwards-compat with node 0.4.x
Stream.Stream = Stream;

// export
module.exports = Stream;
