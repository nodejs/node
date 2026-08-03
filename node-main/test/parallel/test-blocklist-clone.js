'use strict';

const common = require('../common');

const {
  BlockList,
} = require('net');

const assert = require('assert');

const blocklist = new BlockList();
blocklist.addAddress('123.123.123.123');

const mc = new MessageChannel();

mc.port1.onmessage = common.mustCall(({ data }) => {
  assert.notStrictEqual(data, blocklist);
  assert.ok(data.check('123.123.123.123'));
  assert.ok(!data.check('123.123.123.124'));

  data.addAddress('123.123.123.124');
  assert.ok(blocklist.check('123.123.123.124'));
  assert.ok(data.check('123.123.123.124'));

  mc.port1.close();
});

mc.port2.postMessage(blocklist);
