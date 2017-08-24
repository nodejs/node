'use strict';
const common = require('../common');
const net = require('net');

const block = () => {
  new net.Socket({
    fd: 'invalid'
  });
};

const err = {
  code: 'ERR_INVALID_FD_TYPE',
  type: TypeError
};

common.expectsError(block, err);
