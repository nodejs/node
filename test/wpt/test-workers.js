'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('workers');

runner.setFlags(['--experimental-eventsource', '--experimental-fileapi']);
runner.pretendGlobalThisAs('Window');
runner.runJsTests();
