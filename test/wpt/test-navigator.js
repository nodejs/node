'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('html/webappapis/system-state-and-capabilities/the-navigator-object');

runner.runJsTests();
