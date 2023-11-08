'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('compression');

runner.runJsTests();
