'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('dom/abort');

runner.runJsTests();
