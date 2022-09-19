'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('webmessaging/broadcastchannel');

runner.setInitScript(`
  const { BroadcastChannel } = require('worker_threads');
  global.BroadcastChannel = BroadcastChannel;
`);

runner.runJsTests();
