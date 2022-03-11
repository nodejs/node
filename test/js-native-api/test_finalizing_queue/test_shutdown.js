'use strict';
// The test verifies that the finalizing queue is processed
// by the napi_env on shutdown.

const common = require('../../common');
const test = require(`./build/${common.buildType}/test_finalizing_queue`);

test.createObject();
test.createObject();
test.createObject();
