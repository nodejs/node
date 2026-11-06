'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('FileAPI/reading-data-section');
runner.setFlags(['--experimental-fileapi']);

runner.runJsTests();
