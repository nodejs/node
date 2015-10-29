'use strict';
var common = require('../common');
var assert = require('assert'),
    dns = require('dns'),
    net = require('net'),
    isIP = net.isIP,
    isIPv6 = net.isIPv6;
var util = require('util');

var expected = 0,
    completed = 0,
    running = false,
    queue = [];

if (!common.hasIPv6) {
  console.log('1..0 # Skipped: this test, no IPv6 support');
  return;
}

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

TEST(function test_resolve6(done) {
  var req = dns.resolve6('ipv6.google.com', function(err, ips) {
    if (err) throw err;

    assert.ok(ips.length > 0);

    for (var i = 0; i < ips.length; i++) {
      assert.ok(isIPv6(ips[i]));
    }

    done();
  });

  checkWrap(req);
});

TEST(function test_reverse_ipv6(done) {
  var req = dns.reverse('2001:4860:4860::8888', function(err, domains) {
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

TEST(function test_lookup_ipv6_explicit(done) {
  var req = dns.lookup('ipv6.google.com', 6, function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  });

  checkWrap(req);
});

/* This ends up just being too problematic to test
TEST(function test_lookup_ipv6_implicit(done) {
  var req = dns.lookup('ipv6.google.com', function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  });

  checkWrap(req);
});
*/

TEST(function test_lookup_ipv6_explicit_object(done) {
  var req = dns.lookup('ipv6.google.com', {
    family: 6
  }, function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  });

  checkWrap(req);
});

TEST(function test_lookup_ipv6_hint(done) {
  var req = dns.lookup('www.google.com', {
    family: 6,
    hints: dns.V4MAPPED
  }, function(err, ip, family) {
    if (err) {
      // FreeBSD does not support V4MAPPED
      if (process.platform === 'freebsd') {
        assert(err instanceof Error);
        assert.strictEqual(err.code, 'EAI_BADFLAGS');
        assert.strictEqual(err.hostname, 'www.google.com');
        assert.ok(/getaddrinfo EAI_BADFLAGS/.test(err.message));
        done();
        return;
      } else {
        throw err;
      }
    }
    assert.ok(net.isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  });

  checkWrap(req);
});

TEST(function test_lookup_ip_ipv6(done) {
  var req = dns.lookup('::1', function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  });

  checkWrap(req);
});

TEST(function test_lookup_all_ipv6(done) {
  var req = dns.lookup('www.google.com', {all: true, family: 6},
                       function(err, ips) {
    if (err) throw err;
    assert.ok(Array.isArray(ips));
    assert.ok(ips.length > 0);

    ips.forEach(function(ip) {
      assert.ok(isIPv6(ip.address),
                'Invalid IPv6: ' + ip.address.toString());
      assert.strictEqual(ip.family, 6);
    });

    done();
  });

  checkWrap(req);
});

TEST(function test_lookupservice_ip_ipv6(done) {
  var req = dns.lookupService('::1', 80, function(err, host, service) {
    if (err) throw err;
    assert.equal(typeof host, 'string');
    assert(host);

    /*
     * Retrieve the actual HTTP service name as setup on the host currently
     * running the test by reading it from /etc/services. This is not ideal,
     * as the service name lookup could use another mechanism (e.g nscd), but
     * it's already better than hardcoding it.
     */
    var httpServiceName = common.getServiceName(80, 'tcp');
    if (!httpServiceName) {
      /*
       * Couldn't find service name, reverting to the most sensible default
       * for port 80.
       */
      httpServiceName = 'http';
    }

    assert.strictEqual(service, httpServiceName);

    done();
  });

  checkWrap(req);
});

/* Disabled because it appears to be not working on linux. */
/* TEST(function test_lookup_localhost_ipv6(done) {
  var req = dns.lookup('localhost', 6, function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  });

  checkWrap(req);
}); */

process.on('exit', function() {
  console.log(completed + ' tests completed');
  assert.equal(running, false);
  assert.strictEqual(expected, completed);
});
