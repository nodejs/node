// Flags: --expose-internals --no-warnings
'use strict';

// The heap limits of green threads apply, each of them on its own too.

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

const {
  GreenThread,
  kEventMessage,
  kEventError,
  kEventExit,
  kFlagTestingGlobals,
} = internalBinding('green_thread');

const kMB = 1024 * 1024;

// Resolves with the largest size of the new space while a green thread with
// the given limits allocates short-lived objects.
function maxNewSpaceSize(maxOldGenerationSizeMb, maxYoungGenerationSizeMb) {
  const handle = new GreenThread(`
    let max = 0;
    let objects = [];
    for (let i = 0; i < 1e6; i++) {
      objects.push({ i });
      if (objects.length > 1000) objects = [];
      if (i % 10000 === 0) {
        max = Math.max(max, __greenThreadInfo().heapSpaces.new_space.size);
      }
    }
    postMessage(max);
    close();
  `, 'test.js', maxOldGenerationSizeMb, maxYoungGenerationSizeMb,
                                 kFlagTestingGlobals);
  return new Promise((resolve, reject) => {
    let result;
    handle.onevent = common.mustCallAtLeast((type, id, ok, value) => {
      if (type === kEventMessage) {
        result = value;
      } else if (type === kEventError) {
        reject(value);
      } else if (type === kEventExit) {
        handle.close();
        resolve(result);
      }
    });
  });
}

(async () => {
  // Without a limit for the old generation, the limit for the young one
  // used to be replaced by the default.
  assert(await maxNewSpaceSize(0, 1) <= kMB);
  assert(await maxNewSpaceSize(64, 1) <= kMB);
})().then(common.mustCall());
