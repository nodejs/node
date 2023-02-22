'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { RawSession } = require('inspector');

const session = new RawSession();

session.connect();

session.on('message', (result) => {
  const data = JSON.parse(result);
  assert.ok(data.id === 1);
  assert.ok(data.result.result.type === 'number');
  assert.ok(data.result.result.value === 2);
});

const data = JSON.stringify({
  method: 'Runtime.evaluate',
  id: 1,
  params: { expression: '1 + 1' }
});

session.post(data);
