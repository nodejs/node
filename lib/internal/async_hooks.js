'use strict';

const async_wrap = process.binding('async_wrap');

/* Both these arrays are used to communicate between JS and C++ with as little
 * overhead as possible.
 *
 * async_hook_fields is a Uint32Array() that communicates the number of each
 * type of active hooks of each type and wraps the uin32_t array of
 * node::Environment::AsyncHooks::fields_.
 *
 * async_uid_fields is a Float64Array() that contains the async/trigger ids for
 * several operations. These fields are as follows:
 *   kCurrentAsyncId: The async id of the current execution stack.
 *   kCurrentTriggerId: The trigger id of the current execution stack.
 *   kAsyncUidCntr: Counter that tracks the unique ids given to new resources.
 *   kInitTriggerId: Written to just before creating a new resource, so the
 *    constructor knows what other resource is responsible for its init().
 *    Used this way so the trigger id doesn't need to be passed to every
 *    resource's constructor.
 */
const { async_hook_fields, async_uid_fields } = async_wrap;
const {
  kCurrentTriggerId,
  kCurrentAsyncId,
  kInitTriggerId
} = async_wrap.constants;

const {
  kInit,
  kBefore,
  kAfter,
  kDestroy,
  kTotals,
  kAsyncUidCntr,
} = async_wrap.constants;

const internalJSConstants = {
  kInit,
  kBefore,
  kAfter,
  kDestroy,
  kTotals,
  kAsyncUidCntr,
};

// Return the triggerAsyncId meant for the constructor calling it. It's up to
// the user to safeguard this call and make sure it's zero'd out when the
// constructor is complete.
function initTriggerId() {
  var tId = async_uid_fields[kInitTriggerId];
  // Reset value after it's been called so the next constructor doesn't
  // inherit it by accident.
  async_uid_fields[kInitTriggerId] = 0;
  if (tId <= 0)
    tId = executionAsyncId();
  return tId;
}


function resetTriggerId() {
  async_uid_fields[kInitTriggerId] = 0;
}


function executionAsyncId() {
  return async_uid_fields[kCurrentAsyncId];
}


function triggerAsyncId() {
  return async_uid_fields[kCurrentTriggerId];
}


function triggerIdScopeSync(id, block) {
  let old = async_uid_fields[kInitTriggerId];
  async_uid_fields[kInitTriggerId] = id;
  try {
    const ret = block();
    if (async_uid_fields[kInitTriggerId] !== id) old = null;
    return ret;
  } finally {
    old && (async_uid_fields[kInitTriggerId] = old);
  }
}


module.exports = {
  async_wrap,
  async_hook_fields,
  async_uid_fields,
  internalJSConstants,
  initTriggerId,
  resetTriggerId,
  executionAsyncId,
  triggerAsyncId,
  triggerIdScopeSync,
};
