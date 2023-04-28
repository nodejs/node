'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('wasm/webapi');
runner.runJsTests();
