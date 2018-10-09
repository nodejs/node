// Flags: --expose-internals
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

(async function test() {
  const { strictEqual } = require('assert');
  const { Session } = require('inspector');
  const { promisify } = require('util');

  const session = new Session();
  session.connect();
  session.post = promisify(session.post);
  const result = await session.post('Runtime.evaluate', {
    expression: 'for(;;);',
    timeout: 0
  }).catch((e) => e);
  strictEqual(result.message, 'Execution was terminated');
  session.disconnect();
})();
