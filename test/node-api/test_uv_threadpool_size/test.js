'use strict';
// Addons: test_uv_threadpool_size, test_uv_threadpool_size_vtable

const { addonPath } = require('../../common/addon-test');
const { test } = require(addonPath);

const uvThreadpoolSize = parseInt(process.env.EXPECTED_UV_THREADPOOL_SIZE ||
                                  process.env.UV_THREADPOOL_SIZE, 10) || 4;
test(uvThreadpoolSize);
