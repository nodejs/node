'use strict';

const common = require('../common');
const dns = require('dns');
const assert = require('assert');

var m = dns.resolve('www.example.com', 'A', (err, addresses) => {
  assert.throws(() => {
    dns.setServers([]);
  }, /Cannot call setServers\(\) while resolving./);
});

m.on('complete', common.mustCall(() => {
  dns.setServers([]);
}));
