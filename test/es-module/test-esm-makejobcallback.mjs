'use strict';

// This verifies that the [[HostDefined]] is empty as defined by
// https://tc39.es/ecma262/#sec-hostmakejobcallback.

import assert from 'assert';

await assert.rejects(
  Promise.resolve(`import("../fixtures/empty.js")`).then(eval),
  {
    name: 'TypeError',
    code: 'ERR_INVALID_SCRIPT_CONTEXT',
  },
);
