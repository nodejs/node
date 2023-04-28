'use strict';

const common = require('../common');

const {
  BlockList,
} = require('net');

const {
  ok,
  notStrictEqual,
} = require('assert');

const blocklist = new BlockList();
blocklist.addAddress('123.123.123.123');

const mc = new MessageChannel();

mc.port1.onmessage = common.mustCall(({ data }) => {
  notStrictEqual(data, blocklist);
  ok(data.check('123.123.123.123'));
  ok(!data.check('123.123.123.124'));

  data.addAddress('123.123.123.124');
  ok(blocklist.check('123.123.123.124'));
  ok(data.check('123.123.123.124'));

  mc.port1.close();
});

mc.port2.postMessage(blocklist);
