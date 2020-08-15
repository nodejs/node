'use strict';
require('../common');
const { WPTRunner } = require('../common/wpt');
const runner = new WPTRunner('encoding');

runner.setInitScript(`
  const { MessageChannel } = require('worker_threads');
  global.MessageChannel = MessageChannel;
`);

runner.runJsTests();
