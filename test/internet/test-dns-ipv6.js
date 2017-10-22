'use strict';
const common = require('../common');
const { addresses } = require('../common/internet');
if (!common.hasIPv6)
  common.skip('this test, no IPv6 support');

const assert = require('assert');
const dns = require('dns');
const net = require('net');
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

TEST(function test_resolve6(done) {
  const req = dns.resolve6(
    addresses.INET6_HOST,
    common.mustCall((err, ips) => {
      assert.ifError(err);

      assert.ok(ips.length > 0);

      for (let i = 0; i < ips.length; i++)
        assert.ok(isIPv6(ips[i]));

      done();
    }));

  checkWrap(req);
});

TEST(function test_reverse_ipv6(done) {
  const req = dns.reverse(
    addresses.INET6_IP,
    common.mustCall((err, domains) => {
      assert.ifError(err);

      assert.ok(domains.length > 0);

      for (let i = 0; i < domains.length; i++)
        assert.ok(typeof domains[i] === 'string');

      done();
    }));

  checkWrap(req);
});

TEST(function test_lookup_ipv6_explicit(done) {
  const req = dns.lookup(
    addresses.INET6_HOST,
    6,
    common.mustCall((err, ip, family) => {
      assert.ifError(err);
      assert.ok(isIPv6(ip));
      assert.strictEqual(family, 6);

      done();
    }));

  checkWrap(req);
});

/* This ends up just being too problematic to test
TEST(function test_lookup_ipv6_implicit(done) {
  var req = dns.lookup(addresses.INET6_HOST, function(err, ip, family) {
    assert.ifError(err);
    assert.ok(net.isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  });

  checkWrap(req);
});
*/

TEST(function test_lookup_ipv6_explicit_object(done) {
  const req = dns.lookup(addresses.INET6_HOST, {
    family: 6
  }, common.mustCall((err, ip, family) => {
    assert.ifError(err);
    assert.ok(isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  }));

  checkWrap(req);
});

TEST(function test_lookup_ipv6_hint(done) {
  const req = dns.lookup(addresses.INET6_HOST, {
    family: 6,
    hints: dns.V4MAPPED
  }, common.mustCall((err, ip, family) => {
    if (err) {
      // FreeBSD does not support V4MAPPED
      if (common.isFreeBSD) {
        assert(err instanceof Error);
        assert.strictEqual(err.code, 'EAI_BADFLAGS');
        assert.strictEqual(err.hostname, addresses.INET_HOST);
        assert.ok(/getaddrinfo EAI_BADFLAGS/.test(err.message));
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

TEST(function test_lookup_ip_ipv6(done) {
  const req = dns.lookup(
    '::1',
    common.mustCall((err, ip, family) => {
      assert.ifError(err);
      assert.ok(isIPv6(ip));
      assert.strictEqual(family, 6);

      done();
    }));

  checkWrap(req);
});

TEST(function test_lookup_all_ipv6(done) {
  const req = dns.lookup(
    addresses.INET6_HOST,
    { all: true, family: 6 },
    common.mustCall((err, ips) => {
      assert.ifError(err);
      assert.ok(Array.isArray(ips));
      assert.ok(ips.length > 0);

      ips.forEach((ip) => {
        assert.ok(isIPv6(ip.address),
                  `Invalid IPv6: ${ip.address.toString()}`);
        assert.strictEqual(ip.family, 6);
      });

      done();
    })
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
    })
  );

  checkWrap(req);
});

/* Disabled because it appears to be not working on linux. */
/* TEST(function test_lookup_localhost_ipv6(done) {
  var req = dns.lookup('localhost', 6, function(err, ip, family) {
    assert.ifError(err);
    assert.ok(net.isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  });

  checkWrap(req);
}); */
