// Flags: --inspect=0

import * as common from '../common/index.mjs';

common.skipIfInspectorDisabled();

import inspector from 'node:inspector';
import assert from 'node:assert';

const session = new inspector.Session();
session.connect();

assert.throws(() => inspector.emitProtocolEvent(123), {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "eventName" argument must be of type string. Received type number (123)'
});

assert.throws(() => inspector.emitProtocolEvent('NodeNetwork.requestWillBeSent', 'params'), {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "params" argument must be of type object. Received type string (\'params\')'
});

session.on('NodeNetwork.requestWillBeSent', common.mustCall(({ params }) => {
  assert.deepStrictEqual(params, {
    requestId: 'request-id-1',
    request: {
      url: 'https://nodejs.org/en',
      method: 'GET'
    },
    timestamp: 1000,
    wallTime: 1000,
  });
}));

inspector.emitProtocolEvent('NodeNetwork.requestWillBeSent', {
  requestId: 'request-id-1',
  request: {
    url: 'https://nodejs.org/en',
    method: 'GET'
  },
  timestamp: 1000,
  wallTime: 1000,
  unknown: 'unknown'
});
