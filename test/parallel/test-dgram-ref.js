'use strict';
require('../common');
var dgram = require('dgram');

// should not hang, see #1282
dgram.createSocket('udp4');
dgram.createSocket('udp6');
