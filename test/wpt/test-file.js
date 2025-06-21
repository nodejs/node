'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('FileAPI/file');

runner.runJsTests();
