'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('fetch/api');
runner.pretendGlobalThisAs('Window');
runner.runJsTests();