'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('FileAPI/blob');

runner.runJsTests();
