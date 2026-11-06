'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('workers');

runner.setFlags(['--experimental-eventsource']);
runner.pretendGlobalThisAs('Window');
runner.runJsTests();
