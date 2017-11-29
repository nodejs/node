'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { Session } = require('inspector');

const session = new Session();

common.expectsError(
  () => session.post('Runtime.evaluate', { expression: '2 + 2' }),
  {
    code: 'ERR_INSPECTOR_NOT_CONNECTED',
    type: Error,
    message: 'Session is not connected'
  }
);

assert.doesNotThrow(() => session.connect());

assert.doesNotThrow(
  () => session.post('Runtime.evaluate', { expression: '2 + 2' }));

[1, {}, [], true, Infinity, undefined].forEach((i) => {
  common.expectsError(
    () => session.post(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "method" argument must be of type string. ' +
        `Received type ${typeof i}`
    }
  );
});

[1, true, Infinity].forEach((i) => {
  common.expectsError(
    () => session.post('test', i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "params" argument must be of type Object. ' +
        `Received type ${typeof i}`
    }
  );
});

common.expectsError(
  () => session.connect(),
  {
    code: 'ERR_INSPECTOR_ALREADY_CONNECTED',
    type: Error,
    message: 'The inspector is already connected'
  }
);

assert.doesNotThrow(() => session.disconnect());
assert.doesNotThrow(() => session.disconnect());
