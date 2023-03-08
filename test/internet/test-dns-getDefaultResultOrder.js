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
