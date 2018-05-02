'use strict';

const common = require('../common');

const async_hooks = require('async_hooks');
const assert = require('assert');

common.crashOnUnhandledRejection();

const asyncIds = [];

async_hooks.createHook({
  init: (asyncId, type, triggerAsyncId, resource) => {
    asyncIds.push(`${triggerAsyncId} => ${asyncId}`);
  }
}).enable();

async function main() {
  console.log('hello');
  await null;
  console.log('world');
}

main().then(() => assert.deepStrictEqual(
  asyncIds, [ '1 => 3', '1 => 4', '1 => 5', '3 => 6', '3 => 7' ]));
