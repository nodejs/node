'use strict';
const common = require('../../common');
const { test } = require(`./build/${common.buildType}/test_uv_threadpool_size`);

const uvThreadpoolSize = parseInt(process.env.EXPECTED_UV_THREADPOOL_SIZE ||
                                  process.env.UV_THREADPOOL_SIZE, 10) || 4;
test(uvThreadpoolSize);
