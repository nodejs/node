'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('html/webappapis/structured-clone');

runner.runJsTests();
