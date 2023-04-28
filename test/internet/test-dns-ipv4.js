// Flags: --dns-result-order=ipv4first

'use strict';
const common = require('../common');
const { addresses } = require('../common/internet');
const assert = require('assert');
const dns = require('dns');
const net = require('net');
const util = require('util');
const isIPv4 = net.isIPv4;

const dnsPromises = dns.promises;
let running = false;
const queue = [];

function TEST(f) {
  function next() {
    const f = queue.shift();
    if (f) {
      running = true;
      console.log(f.name);
      f(done);
    }
  }

  function done() {
    running = false;
    process.nextTick(next);
  }

  queue.push(f);

  if (!running) {
    next();
  }
}

function checkWrap(req) {
  assert.ok(typeof req === 'object');
}

TEST(async function test_resolve4(done) {
  function validateResult(res) {
    assert.ok(res.length > 0);

    for (let i = 0; i < res.length; i++) {
      assert.ok(isIPv4(res[i]));
    }
  }

  validateResult(await dnsPromises.resolve4(addresses.INET4_HOST));

  const req = dns.resolve4(
    addresses.INET4_HOST,
    common.mustSucceed((ips) => {
      validateResult(ips);
      done();
    }));

  checkWrap(req);
});

TEST(async function test_reverse_ipv4(done) {
  function validateResult(res) {
    assert.ok(res.length > 0);

    for (let i = 0; i < res.length; i++) {
      assert.ok(res[i]);
      assert.ok(typeof res[i] === 'string');
    }
  }

  validateResult(await dnsPromises.reverse(addresses.INET4_IP));

  const req = dns.reverse(
    addresses.INET4_IP,
    common.mustSucceed((domains) => {
      validateResult(domains);
      done();
    }));

  checkWrap(req);
});

TEST(async function test_lookup_ipv4_explicit(done) {
  function validateResult(res) {
    assert.ok(net.isIPv4(res.address));
    assert.strictEqual(res.family, 4);
  }

  validateResult(await dnsPromises.lookup(addresses.INET4_HOST, 4));

  const req = dns.lookup(
    addresses.INET4_HOST, 4,
    common.mustSucceed((ip, family) => {
      validateResult({ address: ip, family });
      done();
    }));

  checkWrap(req);
});

TEST(async function test_lookup_ipv4_implicit(done) {
  function validateResult(res) {
    assert.ok(net.isIPv4(res.address));
    assert.strictEqual(res.family, 4);
  }

  validateResult(await dnsPromises.lookup(addresses.INET4_HOST));

  const req = dns.lookup(
    addresses.INET4_HOST,
    common.mustSucceed((ip, family) => {
      validateResult({ address: ip, family });
      done();
    }));

  checkWrap(req);
});

TEST(async function test_lookup_ipv4_explicit_object(done) {
  function validateResult(res) {
    assert.ok(net.isIPv4(res.address));
    assert.strictEqual(res.family, 4);
  }

  validateResult(await dnsPromises.lookup(addresses.INET4_HOST, { family: 4 }));

  const req = dns.lookup(addresses.INET4_HOST, {
    family: 4,
  }, common.mustSucceed((ip, family) => {
    validateResult({ address: ip, family });
    done();
  }));

  checkWrap(req);
});

TEST(async function test_lookup_ipv4_hint_addrconfig(done) {
  function validateResult(res) {
    assert.ok(net.isIPv4(res.address));
    assert.strictEqual(res.family, 4);
  }

  validateResult(await dnsPromises.lookup(addresses.INET4_HOST, {
    hints: dns.ADDRCONFIG,
  }));

  const req = dns.lookup(addresses.INET4_HOST, {
    hints: dns.ADDRCONFIG,
  }, common.mustSucceed((ip, family) => {
    validateResult({ address: ip, family });
    done();
  }));

  checkWrap(req);
});

TEST(async function test_lookup_ip_ipv4(done) {
  function validateResult(res) {
    assert.strictEqual(res.address, '127.0.0.1');
    assert.strictEqual(res.family, 4);
  }

  validateResult(await dnsPromises.lookup('127.0.0.1'));

  const req = dns.lookup('127.0.0.1',
                         common.mustSucceed((ip, family) => {
                           validateResult({ address: ip, family });
                           done();
                         }));

  checkWrap(req);
});

TEST(async function test_lookup_localhost_ipv4(done) {
  function validateResult(res) {
    assert.strictEqual(res.address, '127.0.0.1');
    assert.strictEqual(res.family, 4);
  }

  validateResult(await dnsPromises.lookup('localhost', 4));

  const req = dns.lookup('localhost', 4,
                         common.mustSucceed((ip, family) => {
                           validateResult({ address: ip, family });
                           done();
                         }));

  checkWrap(req);
});

TEST(async function test_lookup_all_ipv4(done) {
  function validateResult(res) {
    assert.ok(Array.isArray(res));
    assert.ok(res.length > 0);

    res.forEach((ip) => {
      assert.ok(isIPv4(ip.address));
      assert.strictEqual(ip.family, 4);
    });
  }

  validateResult(await dnsPromises.lookup(addresses.INET4_HOST, {
    all: true,
    family: 4,
  }));

  const req = dns.lookup(
    addresses.INET4_HOST,
    { all: true, family: 4 },
    common.mustSucceed((ips) => {
      validateResult(ips);
      done();
    }),
  );

  checkWrap(req);
});

TEST(async function test_lookupservice_ip_ipv4(done) {
  function validateResult(res) {
    assert.strictEqual(typeof res.hostname, 'string');
    assert(res.hostname);
    assert(['http', 'www', '80'].includes(res.service));
  }

  validateResult(await dnsPromises.lookupService('127.0.0.1', 80));

  const req = dns.lookupService(
    '127.0.0.1', 80,
    common.mustSucceed((hostname, service) => {
      validateResult({ hostname, service });
      done();
    }),
  );

  checkWrap(req);
});

TEST(function test_lookupservice_ip_ipv4_promise(done) {
  util.promisify(dns.lookupService)('127.0.0.1', 80)
    .then(common.mustCall(({ hostname, service }) => {
      assert.strictEqual(typeof hostname, 'string');
      assert(hostname.length > 0);
      assert(['http', 'www', '80'].includes(service));
      done();
    }));
});
