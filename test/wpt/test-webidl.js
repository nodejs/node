'use strict';

const common = require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('webidl');

runner.runJsTests();
