// Flags: --experimental-storage-inspection --no-warnings
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const inspector = require('node:inspector/promises');
const assert = require('node:assert');

(async () => {
  const session = new inspector.Session();
  session.connect();

  await session.post('DOMStorage.enable');

  await assert.rejects(
    session.post('DOMStorage.getDOMStorageItems', {
      storageId: {
        isLocalStorage: true,
        securityOrigin: '',
        storageKey: '',
      },
    }),
    {
      code: 'ERR_INSPECTOR_COMMAND',
      message: 'Inspector error -32000: Could not read DOM storage items',
    },
  );

  session.disconnect();
})().then(common.mustCall());
