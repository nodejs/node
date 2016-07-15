'use strict';
const common = require('../common');
var dgram = require('dgram');

var s = dgram.createSocket('udp4');
s.bind();
s.unref();

setTimeout(common.fail, 1000).unref();
