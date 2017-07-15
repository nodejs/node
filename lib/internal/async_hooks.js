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
const { kCurrentTriggerId, kCurrentAsyncId } = async_wrap.constants;

function executionAsyncId() {
  return async_uid_fields[kCurrentAsyncId];
}


function triggerAsyncId() {
  return async_uid_fields[kCurrentTriggerId];
}


function triggerIdScopeSync(id, block) {
  const old = async_uid_fields[kCurrentTriggerId];
  async_uid_fields[kCurrentTriggerId] = id;
  try {
    return block();
  } finally {
    async_uid_fields[kCurrentTriggerId] = old;
  }
}


module.export = {
  async_wrap,
  async_hook_fields,
  async_uid_fields,
  executionAsyncId,
  triggerAsyncId,
  triggerIdScopeSync,
};
