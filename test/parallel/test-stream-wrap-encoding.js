'use strict';
const common = require('../common');
const assert = require('assert');

const StreamWrap = require('_stream_wrap');
const Duplex = require('stream').Duplex;

const stream = new Duplex({
  read: function() {
  },
  write: function() {
  }
});

stream.setEncoding('ascii');

const wrap = new StreamWrap(stream);

wrap.on('error', common.mustCall(function(err) {
  assert(/StringDecoder/.test(err.message));
}));

stream.push('ohai');
