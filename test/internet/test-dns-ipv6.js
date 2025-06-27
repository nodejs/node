'use strict';
const common = require('../common');
const { addresses } = require('../common/internet');
if (!common.hasIPv6)
  common.skip('this test, no IPv6 support');

const assert = require('assert');
const dns = require('dns');
const net = require('net');
const dnsPromises = dns.promises;
const isIPv6 = net.isIPv6;

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

TEST(async function test_resolve6(done) {
  function validateResult(res) {
    assert.ok(res.length > 0);

    for (let i = 0; i < res.length; i++) {
      assert.ok(isIPv6(res[i]));
    }
  }

  validateResult(await dnsPromises.resolve6(addresses.INET6_HOST));

  const req = dns.resolve6(
    addresses.INET6_HOST,
    common.mustSucceed((ips) => {
      validateResult(ips);
      done();
    }));

  checkWrap(req);
});

TEST(async function test_reverse_ipv6(done) {
  function validateResult(res) {
    assert.ok(res.length > 0);

    for (let i = 0; i < res.length; i++) {
      assert.ok(typeof res[i] === 'string');
    }
  }

  validateResult(await dnsPromises.reverse(addresses.INET6_IP));

  const req = dns.reverse(
    addresses.INET6_IP,
    common.mustSucceed((domains) => {
      validateResult(domains);
      done();
    }));

  checkWrap(req);
});

TEST(async function test_lookup_ipv6_explicit(done) {
  function validateResult(res) {
    assert.ok(isIPv6(res.address));
    assert.strictEqual(res.family, 6);
  }

  validateResult(await dnsPromises.lookup(addresses.INET6_HOST, 6));

  const req = dns.lookup(
    addresses.INET6_HOST,
    6,
    common.mustSucceed((ip, family) => {
      validateResult({ address: ip, family });
      done();
    }));

  checkWrap(req);
});

// This ends up just being too problematic to test
// TEST(function test_lookup_ipv6_implicit(done) {
//   var req = dns.lookup(addresses.INET6_HOST, function(err, ip, family) {
//     assert.ifError(err);
//     assert.ok(net.isIPv6(ip));
//     assert.strictEqual(family, 6);

//     done();
//   });

//   checkWrap(req);
// });

TEST(async function test_lookup_ipv6_explicit_object(done) {
  function validateResult(res) {
    assert.ok(isIPv6(res.address));
    assert.strictEqual(res.family, 6);
  }

  validateResult(await dnsPromises.lookup(addresses.INET6_HOST, { family: 6 }));

  const req = dns.lookup(addresses.INET6_HOST, {
    family: 6,
  }, common.mustSucceed((ip, family) => {
    validateResult({ address: ip, family });
    done();
  }));

  checkWrap(req);
});

TEST(function test_lookup_ipv6_hint(done) {
  const req = dns.lookup(addresses.INET6_HOST, {
    family: 6,
    hints: dns.V4MAPPED,
  }, common.mustCall((err, ip, family) => {
    if (err) {
      // FreeBSD does not support V4MAPPED
      if (common.isFreeBSD) {
        assert(err instanceof Error);
        assert.strictEqual(err.code, 'EAI_BADFLAGS');
        assert.strictEqual(err.hostname, addresses.INET_HOST);
        assert.match(err.message, /getaddrinfo EAI_BADFLAGS/);
        done();
        return;
      }

      assert.ifError(err);
    }

    assert.ok(isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  }));

  checkWrap(req);
});

TEST(async function test_lookup_ip_ipv6(done) {
  function validateResult(res) {
    assert.ok(isIPv6(res.address));
    assert.strictEqual(res.family, 6);
  }

  validateResult(await dnsPromises.lookup('::1'));

  const req = dns.lookup(
    '::1',
    common.mustSucceed((ip, family) => {
      validateResult({ address: ip, family });
      done();
    }));

  checkWrap(req);
});

TEST(async function test_lookup_all_ipv6(done) {
  function validateResult(res) {
    assert.ok(Array.isArray(res));
    assert.ok(res.length > 0);

    res.forEach((ip) => {
      assert.ok(isIPv6(ip.address),
                `Invalid IPv6: ${ip.address.toString()}`);
      assert.strictEqual(ip.family, 6);
    });
  }

  validateResult(await dnsPromises.lookup(addresses.INET6_HOST, {
    all: true,
    family: 6,
  }));

  const req = dns.lookup(
    addresses.INET6_HOST,
    { all: true, family: 6 },
    common.mustSucceed((ips) => {
      validateResult(ips);
      done();
    }),
  );

  checkWrap(req);
});

TEST(function test_lookupservice_ip_ipv6(done) {
  const req = dns.lookupService(
    '::1', 80,
    common.mustCall((err, host, service) => {
      if (err) {
        // Not skipping the test, rather checking an alternative result,
        // i.e. that ::1 may not be configured (e.g. in /etc/hosts)
        assert.strictEqual(err.code, 'ENOTFOUND');
        return done();
      }
      assert.strictEqual(typeof host, 'string');
      assert(host);
      assert(['http', 'www', '80'].includes(service));
      done();
    }),
  );

  checkWrap(req);
});

// Disabled because it appears to be not working on Linux.
// TEST(function test_lookup_localhost_ipv6(done) {
//   var req = dns.lookup('localhost', 6, function(err, ip, family) {
//     assert.ifError(err);
//     assert.ok(net.isIPv6(ip));
//     assert.strictEqual(family, 6);
//
//     done();
//   });
//
//   checkWrap(req);
// });
