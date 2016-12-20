'use strict';
const common = require('../common');
const assert = require('assert');
const dns = require('dns');
const net = require('net');
const isIPv4 = net.isIPv4;

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

TEST(function test_resolve4(done) {
  const req = dns.resolve4('www.google.com',
    common.mustCall((err, ips) => {
      assert.ifError(err);

      assert.ok(ips.length > 0);

      for (let i = 0; i < ips.length; i++) {
        assert.ok(isIPv4(ips[i]));
      }

      done();
    }));

  checkWrap(req);
});

TEST(function test_reverse_ipv4(done) {
  const req = dns.reverse('8.8.8.8',
    common.mustCall((err, domains) => {
      assert.ifError(err);

      assert.ok(domains.length > 0);

      for (let i = 0; i < domains.length; i++) {
        assert.ok(domains[i]);
        assert.ok(typeof domains[i] === 'string');
      }

      done();
    }));

  checkWrap(req);
});

TEST(function test_lookup_ipv4_explicit(done) {
  const req = dns.lookup('www.google.com', 4,
    common.mustCall((err, ip, family) => {
      assert.ifError(err);
      assert.ok(net.isIPv4(ip));
      assert.strictEqual(family, 4);

      done();
    }));

  checkWrap(req);
});

TEST(function test_lookup_ipv4_implicit(done) {
  const req = dns.lookup('www.google.com',
    common.mustCall((err, ip, family) => {
      assert.ifError(err);
      assert.ok(net.isIPv4(ip));
      assert.strictEqual(family, 4);

      done();
    }));

  checkWrap(req);
});

TEST(function test_lookup_ipv4_explicit_object(done) {
  const req = dns.lookup('www.google.com', {
    family: 4
  }, common.mustCall((err, ip, family) => {
    assert.ifError(err);
    assert.ok(net.isIPv4(ip));
    assert.strictEqual(family, 4);

    done();
  }));

  checkWrap(req);
});

TEST(function test_lookup_ipv4_hint_addrconfig(done) {
  const req = dns.lookup('www.google.com', {
    hints: dns.ADDRCONFIG
  }, common.mustCall((err, ip, family) => {
    assert.ifError(err);
    assert.ok(net.isIPv4(ip));
    assert.strictEqual(family, 4);

    done();
  }));

  checkWrap(req);
});

TEST(function test_lookup_ip_ipv4(done) {
  const req = dns.lookup('127.0.0.1',
    common.mustCall((err, ip, family) => {
      assert.ifError(err);
      assert.strictEqual(ip, '127.0.0.1');
      assert.strictEqual(family, 4);

      done();
    }));

  checkWrap(req);
});

TEST(function test_lookup_localhost_ipv4(done) {
  const req = dns.lookup('localhost', 4,
    common.mustCall((err, ip, family) => {
      assert.ifError(err);
      assert.strictEqual(ip, '127.0.0.1');
      assert.strictEqual(family, 4);

      done();
    }));

  checkWrap(req);
});

TEST(function test_lookup_all_ipv4(done) {
  const req = dns.lookup(
    'www.google.com',
    {all: true, family: 4},
    common.mustCall((err, ips) => {
      assert.ifError(err);
      assert.ok(Array.isArray(ips));
      assert.ok(ips.length > 0);

      ips.forEach((ip) => {
        assert.ok(isIPv4(ip.address));
        assert.strictEqual(ip.family, 4);
      });

      done();
    }
  ));

  checkWrap(req);
});

TEST(function test_lookupservice_ip_ipv4(done) {
  const req = dns.lookupService('127.0.0.1', 80,
    common.mustCall((err, host, service) => {
      assert.ifError(err);
      assert.strictEqual(typeof host, 'string');
      assert(host);
      assert(['http', 'www', '80'].includes(service));
      done();
    }));

  checkWrap(req);
});
