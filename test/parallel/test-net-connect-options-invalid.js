'use strict';
const common = require('../common');
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
      ...common.localhostIPv4,
      [invalidKey]: true
    };
    const message = `The property 'options.${invalidKey}' is not supported. Received true`;

    assert.throws(() => {
      net.createConnection(option);
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
      message: new RegExp(message)
    });
  });
}
