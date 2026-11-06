'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('workers');

runner.setFlags(['--experimental-fileapi']);
runner.pretendGlobalThisAs('Window');
runner.runJsTests();
