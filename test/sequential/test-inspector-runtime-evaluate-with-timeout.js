'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

(async function test() {
  const assert = require('assert');
  const { Session } = require('inspector');
  const { promisify } = require('util');

  const session = new Session();
  session.connect();
  session.post = promisify(session.post);
  await assert.rejects(
    session.post('Runtime.evaluate', {
      expression: 'for(;;);',
      timeout: 0
    }),
    {
      code: 'ERR_INSPECTOR_COMMAND',
      message: 'Inspector error -32000: Execution was terminated'
    }
  );
  session.disconnect();
})().then(common.mustCall());
