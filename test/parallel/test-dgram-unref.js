'use strict';
const common = require('../common');
const dgram = require('dgram');

const s = dgram.createSocket('udp4');
s.bind();
s.unref();

setTimeout(common.mustNotCall(), 1000).unref();
