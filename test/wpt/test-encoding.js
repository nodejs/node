'use strict';
require('../common');
const { MessageChannel } = require('worker_threads');
const { WPTRunner } = require('../common/wpt');
const runner = new WPTRunner('encoding');

// Copy global descriptors from the global object
runner.copyGlobalsFromObject(global, ['TextDecoder', 'TextEncoder']);

runner.defineGlobal('MessageChannel', {
  get() {
    return MessageChannel;
  }
});

runner.runJsTests();
