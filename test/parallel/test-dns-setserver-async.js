// should not crash
// see https://github.com/iojs/io.js/issues/894 for what this is trying
// to test

var common = require('../common');
var assert = require('assert');
var dns = require('dns');

dns.resolve4('www.microsoft.com', function (err, result) {
  dns.setServers([ '8.8.8.9' ]);
  dns.resolve4('test.com', function (err, result) {});
});
dns.setServers([ '8.8.8.8' ]);
