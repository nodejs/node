'use strict';

const { expectWarning } = require('../common');
const net = require('net');

expectWarning(
  'DeprecationWarning',
  'net._setSimultaneousAccepts() is deprecated and will be removed.',
  'DEP0121');

net._setSimultaneousAccepts();
net._setSimultaneousAccepts();
