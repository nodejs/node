'use strict';

const common = require('../common');

const dns = require('dns');

dns.lookup('localhost', -0, common.mustCall());
dns.lookup('localhost', { family: -0 }, common.mustCall());
dns.lookupService('127.0.0.1', -0, common.mustCall());

new dns.Resolver({ timeout: -0 });
new dns.Resolver({ maxTimeout: -0 });

dns.promises.lookup('localhost', { family: -0 }).then(common.mustCall());
