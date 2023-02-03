'use strict';

require('../common');
const assert = require('assert');
const dnsPromises = require('dns/promises');
const dns = require('dns');

assert.strictEqual(dnsPromises, dns.promises);

assert.strictEqual(dnsPromises.NODATA, dns.NODATA);
assert.strictEqual(dnsPromises.FORMERR, dns.FORMERR);
assert.strictEqual(dnsPromises.SERVFAIL, dns.SERVFAIL);
assert.strictEqual(dnsPromises.NOTFOUND, dns.NOTFOUND);
assert.strictEqual(dnsPromises.NOTIMP, dns.NOTIMP);
assert.strictEqual(dnsPromises.REFUSED, dns.REFUSED);
assert.strictEqual(dnsPromises.BADQUERY, dns.BADQUERY);
assert.strictEqual(dnsPromises.BADNAME, dns.BADNAME);
assert.strictEqual(dnsPromises.BADFAMILY, dns.BADFAMILY);
assert.strictEqual(dnsPromises.BADRESP, dns.BADRESP);
assert.strictEqual(dnsPromises.CONNREFUSED, dns.CONNREFUSED);
assert.strictEqual(dnsPromises.TIMEOUT, dns.TIMEOUT);
assert.strictEqual(dnsPromises.EOF, dns.EOF);
assert.strictEqual(dnsPromises.FILE, dns.FILE);
assert.strictEqual(dnsPromises.NOMEM, dns.NOMEM);
assert.strictEqual(dnsPromises.DESTRUCTION, dns.DESTRUCTION);
assert.strictEqual(dnsPromises.BADSTR, dns.BADSTR);
assert.strictEqual(dnsPromises.BADFLAGS, dns.BADFLAGS);
assert.strictEqual(dnsPromises.NONAME, dns.NONAME);
assert.strictEqual(dnsPromises.BADHINTS, dns.BADHINTS);
assert.strictEqual(dnsPromises.NOTINITIALIZED, dns.NOTINITIALIZED);
assert.strictEqual(dnsPromises.LOADIPHLPAPI, dns.LOADIPHLPAPI);
assert.strictEqual(dnsPromises.ADDRGETNETWORKPARAMS, dns.ADDRGETNETWORKPARAMS);
assert.strictEqual(dnsPromises.CANCELLED, dns.CANCELLED);
