'use strict';
require('../common');
const assert = require('assert');
const dns = require('dns');
const net = require('net');
const isIPv4 = net.isIPv4;

let expected = 0;
let completed = 0;
let running = false;
const queue = [];

function TEST(f) {
  function next() {
    var f = queue.shift();
    if (f) {
      running = true;
      console.log(f.name);
      f(done);
    }
  }

  function done() {
    running = false;
    completed++;
    process.nextTick(next);
  }

  expected++;
  queue.push(f);

  if (!running) {
    next();
  }
}

function checkWrap(req) {
  assert.ok(typeof req === 'object');
}

TEST(function test_resolve4(done) {
  var req = dns.resolve4('www.google.com', function(err, ips) {
    if (err) throw err;

    assert.ok(ips.length > 0);

    for (var i = 0; i < ips.length; i++) {
      assert.ok(isIPv4(ips[i]));
    }

    done();
  });

  checkWrap(req);
});

TEST(function test_reverse_ipv4(done) {
  var req = dns.reverse('8.8.8.8', function(err, domains) {
    if (err) throw err;

    assert.ok(domains.length > 0);

    for (var i = 0; i < domains.length; i++) {
      assert.ok(domains[i]);
      assert.ok(typeof domains[i] === 'string');
    }

    done();
  });

  checkWrap(req);
});

TEST(function test_lookup_ipv4_explicit(done) {
  var req = dns.lookup('www.google.com', 4, function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv4(ip));
    assert.strictEqual(family, 4);

    done();
  });

  checkWrap(req);
});

TEST(function test_lookup_ipv4_implicit(done) {
  var req = dns.lookup('www.google.com', function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv4(ip));
    assert.strictEqual(family, 4);

    done();
  });

  checkWrap(req);
});

TEST(function test_lookup_ipv4_explicit_object(done) {
  var req = dns.lookup('www.google.com', {
    family: 4
  }, function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv4(ip));
    assert.strictEqual(family, 4);

    done();
  });

  checkWrap(req);
});

TEST(function test_lookup_ipv4_hint_addrconfig(done) {
  var req = dns.lookup('www.google.com', {
    hints: dns.ADDRCONFIG
  }, function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv4(ip));
    assert.strictEqual(family, 4);

    done();
  });

  checkWrap(req);
});

TEST(function test_lookup_ip_ipv4(done) {
  var req = dns.lookup('127.0.0.1', function(err, ip, family) {
    if (err) throw err;
    assert.strictEqual(ip, '127.0.0.1');
    assert.strictEqual(family, 4);

    done();
  });

  checkWrap(req);
});

TEST(function test_lookup_localhost_ipv4(done) {
  var req = dns.lookup('localhost', 4, function(err, ip, family) {
    if (err) throw err;
    assert.strictEqual(ip, '127.0.0.1');
    assert.strictEqual(family, 4);

    done();
  });

  checkWrap(req);
});

TEST(function test_lookup_all_ipv4(done) {
  var req = dns.lookup(
    'www.google.com',
    {all: true, family: 4},
    function(err, ips) {
      if (err) throw err;
      assert.ok(Array.isArray(ips));
      assert.ok(ips.length > 0);

      ips.forEach(function(ip) {
        assert.ok(isIPv4(ip.address));
        assert.strictEqual(ip.family, 4);
      });

      done();
    }
  );

  checkWrap(req);
});

TEST(function test_lookupservice_ip_ipv4(done) {
  var req = dns.lookupService('127.0.0.1', 80, function(err, host, service) {
    if (err) throw err;
    assert.equal(typeof host, 'string');
    assert(host);
    assert(['http', 'www', '80'].includes(service));
    done();
  });

  checkWrap(req);
});

process.on('exit', function() {
  console.log(completed + ' tests completed');
  assert.equal(running, false);
  assert.strictEqual(expected, completed);
});
