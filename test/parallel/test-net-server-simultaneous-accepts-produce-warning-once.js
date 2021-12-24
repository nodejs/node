'use strict';

const { expectWarning, mustCall, mustNotCall } = require('../common');
const net = require('net');

expectWarning(
  'DeprecationWarning',
  'net._setSimultaneousAccepts() is deprecated and will be removed.',
  'DEP0121');

process.on('warning', mustCall(() => {
  process.on('warning', mustNotCall());
}));
net._setSimultaneousAccepts();
net._setSimultaneousAccepts();
