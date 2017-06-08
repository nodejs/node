'use strict';

const common = require('../common');
const timeoutTest = require('./test-http-server-keep-alive-timeout');
timeoutTest.testServerKeepAliveTimeout(common, true);
