'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('user-timing');

runner.pretendGlobalThisAs('Window');

runner.runJsTests();
