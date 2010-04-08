require('../common');

var dns = require("dns");


// Try resolution without callback

dns.getHostByName('localhost', function (error, result) {
   p(result);
   assert.deepEqual(['127.0.0.1'], result);
});

dns.getHostByName('127.0.0.1', function (error, result) {
   p(result);
   assert.deepEqual(['127.0.0.1'], result);
});

dns.lookup(null, function (error, result, addressType) {
   assert.equal(null, result);
   assert.equal(4, addressType);
});

dns.lookup('127.0.0.1', function (error, result, addressType) {
   assert.equal('127.0.0.1', result);
   assert.equal(4, addressType);
});

dns.lookup('::1', function (error, result, addressType) {
   assert.equal('::1', result);
   assert.equal(6, addressType);
});

dns.lookup('ipv6.google.com', function (error, result, addressType) {
   if (error) throw error;
   p(arguments);
   //assert.equal('string', typeof result);
   assert.equal(6, addressType);
});
