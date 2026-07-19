'use strict';

const common = require('../../common');
const binding = require(`./build/${common.buildType}/test_uncaught_exception`);
const { testUncaughtException } = require('./uncaught_exception');

testUncaughtException(binding);
