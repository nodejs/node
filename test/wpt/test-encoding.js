'use strict';
require('../common');
const { WPTRunner } = require('../common/wpt');
const runner = new WPTRunner('encoding');

runner.setInitScript(`
  globalThis.location ||= {};
  const { MessageChannel } = require('worker_threads');
  global.MessageChannel = MessageChannel;
`);

runner.runJsTests();
