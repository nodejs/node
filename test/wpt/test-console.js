'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('console');

runner.runJsTests();
