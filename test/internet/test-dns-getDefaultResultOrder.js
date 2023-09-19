'use strict';

const common = require('../common');

const assert = require('assert');
const dns = require('dns');

dns.setDefaultResultOrder('ipv4first');
let dnsOrder = dns.getDefaultResultOrder();
assert.ok(dnsOrder === 'ipv4first');
dns.setDefaultResultOrder('verbatim');
dnsOrder = dns.getDefaultResultOrder();
assert.ok(dnsOrder === 'verbatim');

{
  (async function() {
    const result = await dns.promises.lookup('localhost');
    const result1 = await dns.promises.lookup('localhost', { verbatim: true });
    assert.ok(result !== undefined);
    assert.ok(result1 !== undefined);
    assert.ok(result.address === result1.address);
    assert.ok(result.family === result1.family);
  })().then(common.mustCall());
}
