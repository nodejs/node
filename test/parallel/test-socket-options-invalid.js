'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

{
  const invalidKeys = [
    'objectMode',
    'readableObjectMode',
    'writableObjectMode',
  ];
  invalidKeys.forEach((invalidKey) => {
    const option = {
      [invalidKey]: true
    };
    const message = `The property 'options.${invalidKey}' is not supported. Received true`;

    assert.throws(() => {
      const socket = new net.Socket(option);
      socket.connect({ port: 8080 });
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
      message: new RegExp(message)
    });
  });
}
