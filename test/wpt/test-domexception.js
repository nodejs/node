'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('webidl/ecmascript-binding/es-exceptions');

runner.loadLazyGlobals();

runner.runJsTests();
