// Flags: --inspect=0 --experimental-network-inspection --experimental-storage-inspection
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const inspector = require('node:inspector/promises');
const assert = require('node:assert');

function omit(object, ...keys) {
  const copy = { ...object };
  for (const key of keys) {
    delete copy[key];
  }
  return copy;
}

function networkRequest(overrides = {}) {
  return {
    requestId: 'request-id',
    timestamp: 1000,
    wallTime: 1000,
    request: {
      url: 'https://nodejs.org/en',
      method: 'GET',
      headers: {},
    },
    ...overrides,
  };
}

function networkResponse(overrides = {}) {
  return {
    requestId: 'response-id',
    timestamp: 1000,
    type: 'Other',
    response: {
      url: 'https://nodejs.org/en',
      status: 200,
      statusText: 'OK',
      headers: {},
    },
    ...overrides,
  };
}

function loadingFailed(overrides = {}) {
  return {
    requestId: 'loading-failed-id',
    timestamp: 1000,
    type: 'Document',
    errorText: 'Failed to load resource',
    ...overrides,
  };
}

function loadingFinished(overrides = {}) {
  return {
    requestId: 'loading-finished-id',
    timestamp: 1000,
    ...overrides,
  };
}

function webSocketCreated(overrides = {}) {
  return {
    requestId: 'websocket-created-id',
    url: 'ws://example.com:8080',
    ...overrides,
  };
}

function webSocketClosed(overrides = {}) {
  return {
    requestId: 'websocket-closed-id',
    timestamp: 1000,
    ...overrides,
  };
}

function webSocketResponse(overrides = {}) {
  return {
    requestId: 'websocket-response-id',
    timestamp: 1000,
    response: {
      status: 101,
      statusText: 'Switching Protocols',
      headers: {},
    },
    ...overrides,
  };
}

function storageId(overrides = {}) {
  return {
    securityOrigin: '',
    isLocalStorage: true,
    storageKey: 'node-inspector://default-dom-storage',
    ...overrides,
  };
}

function domStorageItemAdded(overrides = {}) {
  return {
    storageId: storageId(),
    key: 'testKey',
    newValue: 'testValue',
    ...overrides,
  };
}

function domStorageItemRemoved(overrides = {}) {
  return {
    storageId: storageId(),
    key: 'testKey',
    ...overrides,
  };
}

function domStorageItemUpdated(overrides = {}) {
  return {
    storageId: storageId(),
    key: 'testKey',
    oldValue: 'oldValue',
    newValue: 'newValue',
    ...overrides,
  };
}

function registerStorage(overrides = {}) {
  return {
    isLocalStorage: true,
    storageMap: {},
    ...overrides,
  };
}

const NETWORK_ERROR_CASES = [
  [
    'requestWillBeSent',
    omit(networkRequest(), 'requestId'),
    'Missing requestId in event',
  ],
  [
    'requestWillBeSent',
    omit(networkRequest(), 'timestamp'),
    'Missing timestamp in event',
  ],
  [
    'requestWillBeSent',
    omit(networkRequest(), 'wallTime'),
    'Missing wallTime in event',
  ],
  [
    'requestWillBeSent',
    omit(networkRequest(), 'request'),
    'Missing request in event',
  ],
  [
    'requestWillBeSent',
    networkRequest({ request: omit(networkRequest().request, 'url') }),
    'Missing request.url in event',
  ],
  [
    'requestWillBeSent',
    networkRequest({ request: omit(networkRequest().request, 'method') }),
    'Missing request.method in event',
  ],
  [
    'requestWillBeSent',
    networkRequest({ request: omit(networkRequest().request, 'headers') }),
    'Missing request.headers in event',
  ],

  [
    'responseReceived',
    omit(networkResponse(), 'requestId'),
    'Missing requestId in event',
  ],
  [
    'responseReceived',
    omit(networkResponse(), 'timestamp'),
    'Missing timestamp in event',
  ],
  [
    'responseReceived',
    omit(networkResponse(), 'type'),
    'Missing type in event',
  ],
  [
    'responseReceived',
    omit(networkResponse(), 'response'),
    'Missing response in event',
  ],
  [
    'responseReceived',
    networkResponse({ response: omit(networkResponse().response, 'url') }),
    'Missing response.url in event',
  ],
  [
    'responseReceived',
    networkResponse({ response: omit(networkResponse().response, 'status') }),
    'Missing response.status in event',
  ],
  [
    'responseReceived',
    networkResponse({
      response: omit(networkResponse().response, 'statusText'),
    }),
    'Missing response.statusText in event',
  ],
  [
    'responseReceived',
    networkResponse({ response: omit(networkResponse().response, 'headers') }),
    'Missing response.headers in event',
  ],
  [
    'responseReceived',
    networkResponse({
      response: { ...networkResponse().response, headers: { host: 1 } },
    }),
    'Invalid header value in event',
  ],

  [
    'loadingFailed',
    omit(loadingFailed(), 'requestId'),
    'Missing requestId in event',
  ],
  [
    'loadingFailed',
    omit(loadingFailed(), 'timestamp'),
    'Missing timestamp in event',
  ],
  ['loadingFailed', omit(loadingFailed(), 'type'), 'Missing type in event'],
  [
    'loadingFailed',
    omit(loadingFailed(), 'errorText'),
    'Missing errorText in event',
  ],

  [
    'loadingFinished',
    omit(loadingFinished(), 'requestId'),
    'Missing requestId in event',
  ],
  [
    'loadingFinished',
    omit(loadingFinished(), 'timestamp'),
    'Missing timestamp in event',
  ],

  [
    'webSocketCreated',
    omit(webSocketCreated(), 'requestId'),
    'Missing requestId in event',
  ],
  ['webSocketCreated', omit(webSocketCreated(), 'url'), 'Missing url in event'],

  [
    'webSocketClosed',
    omit(webSocketClosed(), 'requestId'),
    'Missing requestId in event',
  ],
  [
    'webSocketClosed',
    omit(webSocketClosed(), 'timestamp'),
    'Missing timestamp in event',
  ],

  [
    'webSocketHandshakeResponseReceived',
    omit(webSocketResponse(), 'requestId'),
    'Missing requestId in event',
  ],
  [
    'webSocketHandshakeResponseReceived',
    omit(webSocketResponse(), 'timestamp'),
    'Missing timestamp in event',
  ],
  [
    'webSocketHandshakeResponseReceived',
    omit(webSocketResponse(), 'response'),
    'Missing response in event',
  ],
  [
    'webSocketHandshakeResponseReceived',
    webSocketResponse({
      response: omit(webSocketResponse().response, 'status'),
    }),
    'Missing response.status in event',
  ],
  [
    'webSocketHandshakeResponseReceived',
    webSocketResponse({
      response: omit(webSocketResponse().response, 'statusText'),
    }),
    'Missing response.statusText in event',
  ],
  [
    'webSocketHandshakeResponseReceived',
    webSocketResponse({
      response: omit(webSocketResponse().response, 'headers'),
    }),
    'Missing response.headers in event',
  ],
];

const DOM_STORAGE_ERROR_CASES = [
  [
    'domStorageItemAdded',
    omit(domStorageItemAdded(), 'storageId'),
    'Missing storageId in event',
  ],
  [
    'domStorageItemAdded',
    domStorageItemAdded({ storageId: omit(storageId(), 'securityOrigin') }),
    'Missing securityOrigin in storageId',
  ],
  [
    'domStorageItemAdded',
    domStorageItemAdded({ storageId: omit(storageId(), 'storageKey') }),
    'Missing storageKey in storageId',
  ],
  [
    'domStorageItemAdded',
    omit(domStorageItemAdded(), 'key'),
    'Missing key in event',
  ],
  [
    'domStorageItemAdded',
    omit(domStorageItemAdded(), 'newValue'),
    'Missing newValue in event',
  ],

  [
    'domStorageItemRemoved',
    omit(domStorageItemRemoved(), 'storageId'),
    'Missing storageId in event',
  ],
  [
    'domStorageItemRemoved',
    omit(domStorageItemRemoved(), 'key'),
    'Missing key in event',
  ],

  [
    'domStorageItemUpdated',
    omit(domStorageItemUpdated(), 'storageId'),
    'Missing storageId in event',
  ],
  [
    'domStorageItemUpdated',
    omit(domStorageItemUpdated(), 'key'),
    'Missing key in event',
  ],
  [
    'domStorageItemUpdated',
    omit(domStorageItemUpdated(), 'oldValue'),
    'Missing oldValue in event',
  ],
  [
    'domStorageItemUpdated',
    omit(domStorageItemUpdated(), 'newValue'),
    'Missing newValue in event',
  ],

  ['domStorageItemsCleared', {}, 'Missing storageId in event'],

  [
    'registerStorage',
    omit(registerStorage(), 'isLocalStorage'),
    'Missing isLocalStorage in event',
  ],
  [
    'registerStorage',
    omit(registerStorage(), 'storageMap'),
    'Missing storageMap in event',
  ],
  [
    'registerStorage',
    registerStorage({
      storageMap: new Proxy(
        {},
        {
          ownKeys() {
            throw new Error('boom');
          },
        },
      ),
    }),
    'Failed to get property names from storageMap',
  ],
  [
    'registerStorage',
    registerStorage({
      storageMap: new Proxy(
        { testKey: 'testValue' },
        {
          get(target, property, receiver) {
            if (property === 'testKey') throw new Error('boom');
            return Reflect.get(target, property, receiver);
          },
        },
      ),
    }),
    'Failed to get value from storageMap',
  ],
];

const DATA_SENT_REQUEST_ID = 'data-sent-id';
const DATA_SENT_ERROR_CASES = [
  [{ finished: false }, 'Missing requestId in event'],
  [{ requestId: DATA_SENT_REQUEST_ID }, 'Missing timestamp in event'],
  [
    { requestId: DATA_SENT_REQUEST_ID, timestamp: 1000 },
    'Missing dataLength in event',
  ],
  [
    { requestId: DATA_SENT_REQUEST_ID, timestamp: 1000, dataLength: 1 },
    'Missing data in event',
  ],
  [
    {
      requestId: DATA_SENT_REQUEST_ID,
      timestamp: 1000,
      dataLength: 1,
      data: {},
    },
    'Expected data to be Uint8Array in event',
  ],
];

const DATA_RECEIVED_ERROR_CASES = [
  [{}, 'Missing requestId in event'],
  [{ requestId: 'data-received-id' }, 'Missing timestamp in event'],
  [
    { requestId: 'data-received-id', timestamp: 1000 },
    'Missing dataLength in event',
  ],
  [
    { requestId: 'data-received-id', timestamp: 1000, dataLength: 1 },
    'Missing encodedDataLength in event',
  ],
  [
    {
      requestId: 'data-received-id',
      timestamp: 1000,
      dataLength: 1,
      encodedDataLength: 1,
    },
    'Missing data in event',
  ],
  [
    {
      requestId: 'data-received-id',
      timestamp: 1000,
      dataLength: 1,
      encodedDataLength: 1,
      data: {},
    },
    'Expected data to be Uint8Array in event',
  ],
];

function assertEventErrors(domain, name, params, message) {
  assert.throws(
    () => inspector[domain][name](params),
    {
      message,
    },
    `Expected ${domain}.${name} to throw`,
  );
}

function startRequest(requestId) {
  inspector.Network.requestWillBeSent(networkRequest({ requestId }));
}

(async () => {
  const session = new inspector.Session();
  session.connect();

  await session.post('Network.enable');
  await session.post('DOMStorage.enable');

  for (const [name, params, message] of NETWORK_ERROR_CASES) {
    assertEventErrors('Network', name, params, message);
  }

  startRequest(DATA_SENT_REQUEST_ID);
  for (const [params, message] of DATA_SENT_ERROR_CASES) {
    assertEventErrors('Network', 'dataSent', params, message);
  }

  for (const [params, message] of DATA_RECEIVED_ERROR_CASES) {
    assertEventErrors('Network', 'dataReceived', params, message);
  }

  for (const [name, params, message] of DOM_STORAGE_ERROR_CASES) {
    assertEventErrors('DOMStorage', name, params, message);
  }
})().then(common.mustCall());
