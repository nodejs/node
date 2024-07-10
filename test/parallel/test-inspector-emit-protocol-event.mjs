// Flags: --inspect=0 --experimental-network-inspection

import * as common from '../common/index.mjs';

common.skipIfInspectorDisabled();

import inspector from 'node:inspector/promises';
import assert from 'node:assert';

const EXPECTED_EVENTS = {
  NodeNetwork: [
    {
      name: 'requestWillBeSent',
      params: {
        requestId: 'request-id-1',
        request: {
          url: 'https://nodejs.org/en',
          method: 'GET'
        },
        timestamp: 1000,
        wallTime: 1000,
      }
    },
    {
      name: 'responseReceived',
      params: {
        requestId: 'request-id-1',
        timestamp: 1000,
      }
    },
    {
      name: 'loadingFinished',
      params: {
        requestId: 'request-id-1',
        timestamp: 1000,
      }
    },
  ]
};

// Check that all domains and events are present in the inspector object.
for (const [domain, events] of Object.entries(EXPECTED_EVENTS)) {
  if (!(domain in inspector)) {
    assert.fail(`Expected domain ${domain} to be present in inspector`);
  }
  const actualEventNames = Object.keys(inspector[domain]);
  const expectedEventNames = events.map((event) => event.name);
  assert.deepStrictEqual(actualEventNames, expectedEventNames, `Expected ${domain} to have events ${expectedEventNames}, but got ${actualEventNames}`);
}

// Check that all events throw when called with a non-object argument.
for (const [domain, events] of Object.entries(EXPECTED_EVENTS)) {
  for (const event of events) {
    assert.throws(() => inspector[domain][event.name]('params'), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "params" argument must be of type object. Received type string (\'params\')'
    });
  }
}

const session = new inspector.Session();
session.connect();

// Check that all events emit the expected parameters.
await session.post('NodeNetwork.enable');
for (const [domain, events] of Object.entries(EXPECTED_EVENTS)) {
  for (const event of events) {
    session.on(`${domain}.${event.name}`, common.mustCall(({ params }) => {
      assert.deepStrictEqual(params, event.params);
    }));
    inspector[domain][event.name](event.params);
  }
}

// Check tht no events are emitted after disabling the domain.
await session.post('NodeNetwork.disable');
session.on('NodeNetwork.requestWillBeSent', common.mustNotCall());
inspector.NodeNetwork.requestWillBeSent({});
