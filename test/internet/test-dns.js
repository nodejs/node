'use strict';
const common = require('../common');
const assert = require('assert');
const dns = require('dns');
const net = require('net');
const isIP = net.isIP;
const isIPv4 = net.isIPv4;
const isIPv6 = net.isIPv6;

var expected = 0;
var completed = 0;
var running = false;
var queue = [];


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
  if (process.oldDNS)
    assert.ok(typeof req === 'object');
}


if (process.oldDNS) {
  TEST(function test_cares_getaddrinfo(done) {
    const cares = process.binding('cares_wrap');
    const req = new cares.GetAddrInfoReqWrap();
    const err = cares.getaddrinfo(req, 'nodejs.org', 4);
    req.oncomplete = common.mustCall(function(err, domains) {
      assert.strictEqual(err, 0);
      assert.ok(Array.isArray(domains));
      assert.ok(domains.length >= 1);
      assert.ok(typeof domains[0] === 'string');
    });
  });
}


TEST(function test_lookup_all_mixed(done) {
  const req = dns.lookup('www.google.com', {all: true}, function(err, ips) {
    if (err) throw err;

    assert.ok(Array.isArray(ips));
    assert.ok(ips.length > 0);

    ips.forEach(function(ip) {
      if (isIPv4(ip.address))
        assert.equal(ip.family, 4);
      else if (isIPv6(ip.address))
        assert.equal(ip.family, 6);
      else
        assert(false);
    });

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_failure(done) {
  const req = dns.lookup('does.not.exist', 4, function(err, ip, family) {
    assert.ok(err instanceof Error);

    assert.strictEqual(err.errno, 'ENOTFOUND');
    assert.ok(!/ENOENT/.test(err.message));
    assert.strictEqual(err.hostname, 'does.not.exist');
    assert.ok(/does\.not\.exist/.test(err.message));

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_failure2(done) {
  const req = dns.lookup('nosuchhostimsure', function(err) {
    assert(err instanceof Error);

    assert.strictEqual(err.code, 'ENOTFOUND');  // Silly error code...
    assert.strictEqual(err.hostname, 'nosuchhostimsure');
    assert.ok(/nosuchhostimsure/.test(err.message));

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_ip_all(done) {
  const req = dns.lookup('127.0.0.1', {all: true}, function(err, ips, family) {
    if (err) throw err;

    assert.ok(Array.isArray(ips));
    assert.ok(ips.length > 0);
    assert.strictEqual(ips[0].address, '127.0.0.1');
    assert.strictEqual(ips[0].family, 4);

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_null(done) {
  const req = dns.lookup(null, function(err, ip, family) {
    if (err) throw err;

    assert.strictEqual(ip, null);
    assert.strictEqual(family, 4);

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_null_all(done) {
  const req = dns.lookup(null, {all: true}, function(err, ips, family) {
    if (err) throw err;

    assert.ok(Array.isArray(ips));
    assert.strictEqual(ips.length, 0);

    done();
  });

  checkWrap(req);
});


TEST(function test_lookupservice_invalid(done) {
  const req = dns.lookupService('1.2.3.4', 80, function(err, host, service) {
    assert(err instanceof Error);

    assert.strictEqual(err.code, 'ENOTFOUND');
    assert.ok(/1\.2\.3\.4/.test(err.message));

    done();
  });

  checkWrap(req);
});


TEST(function test_resolve_failure(done) {
  const req = dns.resolve4('nosuchhostimsure', function(err) {
    assert(err instanceof Error);

    switch (err.code) {
      case 'ENOTFOUND':
      case 'ESERVFAIL':
        break;
      default:
        assert.strictEqual(err.code, 'ENOTFOUND');  // Silly error code...
        break;
    }

    assert.strictEqual(err.hostname, 'nosuchhostimsure');
    assert.ok(/nosuchhostimsure/.test(err.message));

    done();
  });

  checkWrap(req);
});


TEST(function test_resolveCname(done) {
  const req = dns.resolveCname('cname.statdns.net', function(err, names) {
    if (err) throw err;

    assert.equal(names.length, 1);
    assert.strictEqual(names[0], 'www.statdns.net');

    done();
  });

  checkWrap(req);
});


TEST(function test_resolveLoc(done) {
  if (!dns.resolveLoc)
    return done();

  dns.resolveLoc('statdns.net', function(err, records) {
    if (err) throw err;

    assert.equal(records.length, 1);
    assert.deepEqual(records[0], {
      size: 0,
      horizPrec: 22,
      vertPrec: 19,
      latitude: 2336026648,
      longitude: 2165095648,
      altitude: 9999800
    });

    done();
  });
});


TEST(function test_resolveMx(done) {
  const req = dns.resolveMx('gmail.com', function(err, result) {
    if (err) throw err;

    assert.ok(result.length > 0);

    for (let i = 0; i < result.length; i++) {
      const item = result[i];
      assert.ok(item);
      assert.ok(typeof item === 'object');

      assert.ok(item.exchange);
      assert.ok(typeof item.exchange === 'string');

      assert.ok(typeof item.priority === 'number');
    }

    done();
  });

  checkWrap(req);
});


TEST(function test_resolveNaptr(done) {
  const req = dns.resolveNaptr('statdns.net', function(err, result) {
    if (err) throw err;

    assert.equal(result.length, 1);
    assert.deepEqual(result[0], {
      flags: 'u',
      service: 'E2U+web:http',
      regexp: '!^.*$!https://www.statdns.net!',
      replacement: '',
      order: 100,
      preference: 100
    });

    done();
  });

  checkWrap(req);
});


TEST(function test_resolveNs(done) {
  const req = dns.resolveNs('rackspace.com', function(err, names) {
    if (err) throw err;

    assert.ok(names.length > 0);

    for (let i = 0; i < names.length; i++) {
      const name = names[i];
      assert.ok(name);
      assert.ok(typeof name === 'string');
    }

    done();
  });

  checkWrap(req);
});


TEST(function test_reverse_bogus(done) {
  var error;

  try {
    const req = dns.reverse('bogus ip', function() {
      assert.ok(false);
    });
  } catch (e) {
    error = e;
  }

  assert.ok(error instanceof Error);
  assert.strictEqual(error.errno, 'EINVAL');

  done();
});


TEST(function test_reverse_failure(done) {
  const req = dns.reverse('0.0.0.0', function(err) {
    assert(err instanceof Error);

    assert.strictEqual(err.code, 'ENOTFOUND');  // Silly error code...
    assert.strictEqual(err.hostname, '0.0.0.0');
    assert.ok(/0\.0\.0\.0/.test(err.message));

    done();
  });

  checkWrap(req);
});


TEST(function test_resolveSoa(done) {
  const req = dns.resolveSoa('nodejs.org', function(err, result) {
    if (err) throw err;

    assert.ok(result);
    assert.ok(typeof result === 'object');

    assert.ok(typeof result.nsname === 'string');
    assert.ok(result.nsname.length > 0);

    assert.ok(typeof result.hostmaster === 'string');
    assert.ok(result.hostmaster.length > 0);

    assert.ok(typeof result.serial === 'number');
    assert.ok((result.serial > 0) && (result.serial < 4294967295));

    assert.ok(typeof result.refresh === 'number');
    assert.ok((result.refresh > 0) && (result.refresh < 2147483647));

    assert.ok(typeof result.retry === 'number');
    assert.ok((result.retry > 0) && (result.retry < 2147483647));

    assert.ok(typeof result.expire === 'number');
    assert.ok((result.expire > 0) && (result.expire < 2147483647));

    assert.ok(typeof result.minttl === 'number');
    assert.ok((result.minttl >= 0) && (result.minttl < 2147483647));

    done();
  });

  checkWrap(req);
});


TEST(function test_resolveSrv(done) {
  const req = dns.resolveSrv('_sip._tcp.statdns.net', function(err, result) {
    if (err) throw err;

    assert.equal(result.length, 1);
    assert.deepEqual(result[0], {
      priority: 0,
      weight: 5,
      port: 5060,
      name: 'sipserver.statdns.net'
    });

    done();
  });

  checkWrap(req);
});


TEST(function test_resolveSshp(done) {
  if (!dns.resolveSshp)
    return done();

  dns.resolveSshp('statdns.net', function(err, records) {
    if (err) throw err;

    assert.equal(records.length, 1);
    assert.deepEqual(records[0], {
      algorithm: 'DSA',
      fpType: 'SHA1',
      fp: '123456789abcdef67890123456789abcdef67890'
    });

    done();
  });
});


TEST(function test_resolveTxt(done) {
  const req = dns.resolveTxt('statdns.net', function(err, records) {
    if (err) throw err;

    assert.equal(records.length, 1);
    assert.deepEqual(records[0], [
      'Get more information on : http://www.statdns.net'
    ]);

    done();
  });

  checkWrap(req);
});


process.on('exit', function() {
  console.log(completed + ' tests completed');
  assert.equal(running, false);
  assert.strictEqual(expected, completed);
});
