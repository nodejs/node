var common = require('../common');
var assert = require('assert');
var dns = require('dns');

var servers = dns.getServers();
assert.ok(servers.indexOf('8.8.8.8') !== -1);
