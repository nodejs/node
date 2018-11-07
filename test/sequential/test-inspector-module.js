'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

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

session.connect();
session.post('Runtime.evaluate', { expression: '2 + 2' });

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

[1, 'a', {}, [], true, Infinity].forEach((i) => {
  common.expectsError(
    () => session.post('test', {}, i),
    {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError,
      message: 'Callback must be a function'
    }
  );
});

common.expectsError(
  () => session.connect(),
  {
    code: 'ERR_INSPECTOR_ALREADY_CONNECTED',
    type: Error,
    message: 'The inspector session is already connected'
  }
);

session.disconnect();
// Calling disconnect twice should not throw.
session.disconnect();
