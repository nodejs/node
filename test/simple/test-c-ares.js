require('../common');

var dns = require("dns_cares");


// Try resolution without callback

dns.getHostByName('localhost', function (error, result) {
   p(result);
   assert.deepEqual(['127.0.0.1'], result);
});

dns.getHostByName('127.0.0.1', function (error, result) {
   p(result);
   assert.deepEqual(['127.0.0.1'], result);
});

dns.lookup('127.0.0.1', function (error, result, ipVersion) {
   assert.deepEqual('127.0.0.1', result);
   assert.equal(4, ipVersion);
});

dns.lookup('ipv6.google.com', function (error, result, ipVersion) {
   if (error) throw error;
   p(arguments);
   //assert.equal('string', typeof result);
   assert.equal(6, ipVersion);
});
