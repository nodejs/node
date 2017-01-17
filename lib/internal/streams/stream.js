'use strict';

const EE = require('events');
const util = require('util');

function Stream() {
  EE.call(this);
}
util.inherits(Stream, EE);

exports.Stream = Stream;
