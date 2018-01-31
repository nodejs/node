'use strict';
const common = require('../common');
const net = require('net');

common.expectsError(function() { net.createServer('path'); },
                    {
                      code: 'ERR_INVALID_ARG_TYPE',
                      type: TypeError
                    });

common.expectsError(function() { net.createServer(0); },
                    {
                      code: 'ERR_INVALID_ARG_TYPE',
                      type: TypeError
                    });
