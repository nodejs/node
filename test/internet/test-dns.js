'use strict';
const common = require('../common');
const assert = require('assert');
const dns = require('dns');
const net = require('net');
const isIPv4 = net.isIPv4;
const isIPv6 = net.isIPv6;
const util = require('util');

let expected = 0;
let completed = 0;
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
  assert.strictEqual(typeof req, 'object');
}


TEST(function test_reverse_bogus(done) {
  assert.throws(() => {
    dns.reverse('bogus ip', common.mustNotCall());
  }, /^Error: getHostByAddr EINVAL$/);
  done();
});

TEST(function test_resolve4_ttl(done) {
  const req = dns.resolve4('google.com', { ttl: true }, function(err, result) {
    assert.ifError(err);
    assert.ok(result.length > 0);

    for (let i = 0; i < result.length; i++) {
      const item = result[i];
      assert.ok(item);
      assert.strictEqual(typeof item, 'object');
      assert.strictEqual(typeof item.ttl, 'number');
      assert.strictEqual(typeof item.address, 'string');
      assert.ok(item.ttl > 0);
      assert.ok(isIPv4(item.address));
    }

    done();
  });

  checkWrap(req);
});

TEST(function test_resolve6_ttl(done) {
  const req = dns.resolve6('google.com', { ttl: true }, function(err, result) {
    assert.ifError(err);
    assert.ok(result.length > 0);

    for (let i = 0; i < result.length; i++) {
      const item = result[i];
      assert.ok(item);
      assert.strictEqual(typeof item, 'object');
      assert.strictEqual(typeof item.ttl, 'number');
      assert.strictEqual(typeof item.address, 'string');
      assert.ok(item.ttl > 0);
      assert.ok(isIPv6(item.address));
    }

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveMx(done) {
  const req = dns.resolveMx('gmail.com', function(err, result) {
    assert.ifError(err);
    assert.ok(result.length > 0);

    for (let i = 0; i < result.length; i++) {
      const item = result[i];
      assert.ok(item);
      assert.strictEqual(typeof item, 'object');

      assert.ok(item.exchange);
      assert.strictEqual(typeof item.exchange, 'string');

      assert.strictEqual(typeof item.priority, 'number');
    }

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveMx_failure(done) {
  const req = dns.resolveMx('something.invalid', function(err, result) {
    assert.ok(err instanceof Error);
    assert.strictEqual(err.errno, 'ENOTFOUND');

    assert.strictEqual(result, undefined);

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveNs(done) {
  const req = dns.resolveNs('rackspace.com', function(err, names) {
    assert.ifError(err);
    assert.ok(names.length > 0);

    for (let i = 0; i < names.length; i++) {
      const name = names[i];
      assert.ok(name);
      assert.strictEqual(typeof name, 'string');
    }

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveNs_failure(done) {
  const req = dns.resolveNs('something.invalid', function(err, result) {
    assert.ok(err instanceof Error);
    assert.strictEqual(err.errno, 'ENOTFOUND');

    assert.strictEqual(result, undefined);

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveSrv(done) {
  const req = dns.resolveSrv('_jabber._tcp.google.com', function(err, result) {
    assert.ifError(err);
    assert.ok(result.length > 0);

    for (let i = 0; i < result.length; i++) {
      const item = result[i];
      assert.ok(item);
      assert.strictEqual(typeof item, 'object');

      assert.ok(item.name);
      assert.strictEqual(typeof item.name, 'string');

      assert.strictEqual(typeof item.port, 'number');
      assert.strictEqual(typeof item.priority, 'number');
      assert.strictEqual(typeof item.weight, 'number');
    }

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveSrv_failure(done) {
  const req = dns.resolveSrv('something.invalid', function(err, result) {
    assert.ok(err instanceof Error);
    assert.strictEqual(err.errno, 'ENOTFOUND');

    assert.strictEqual(result, undefined);

    done();
  });

  checkWrap(req);
});

TEST(function test_resolvePtr(done) {
  const req = dns.resolvePtr('8.8.8.8.in-addr.arpa', function(err, result) {
    assert.ifError(err);
    assert.ok(result.length > 0);

    for (let i = 0; i < result.length; i++) {
      const item = result[i];
      assert.ok(item);
      assert.strictEqual(typeof item, 'string');
    }

    done();
  });

  checkWrap(req);
});

TEST(function test_resolvePtr_failure(done) {
  const req = dns.resolvePtr('something.invalid', function(err, result) {
    assert.ok(err instanceof Error);
    assert.strictEqual(err.errno, 'ENOTFOUND');

    assert.strictEqual(result, undefined);

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveNaptr(done) {
  const req = dns.resolveNaptr('sip2sip.info', function(err, result) {
    assert.ifError(err);
    assert.ok(result.length > 0);

    for (let i = 0; i < result.length; i++) {
      const item = result[i];
      assert.ok(item);
      assert.strictEqual(typeof item, 'object');

      assert.strictEqual(typeof item.flags, 'string');
      assert.strictEqual(typeof item.service, 'string');
      assert.strictEqual(typeof item.regexp, 'string');
      assert.strictEqual(typeof item.replacement, 'string');
      assert.strictEqual(typeof item.order, 'number');
      assert.strictEqual(typeof item.preference, 'number');
    }

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveNaptr_failure(done) {
  const req = dns.resolveNaptr('something.invalid', function(err, result) {
    assert.ok(err instanceof Error);
    assert.strictEqual(err.errno, 'ENOTFOUND');

    assert.strictEqual(result, undefined);

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveSoa(done) {
  const req = dns.resolveSoa('nodejs.org', function(err, result) {
    assert.ifError(err);
    assert.ok(result);
    assert.strictEqual(typeof result, 'object');

    assert.strictEqual(typeof result.nsname, 'string');
    assert.ok(result.nsname.length > 0);

    assert.strictEqual(typeof result.hostmaster, 'string');
    assert.ok(result.hostmaster.length > 0);

    assert.strictEqual(typeof result.serial, 'number');
    assert.ok((result.serial > 0) && (result.serial < 4294967295));

    assert.strictEqual(typeof result.refresh, 'number');
    assert.ok((result.refresh > 0) && (result.refresh < 2147483647));

    assert.strictEqual(typeof result.retry, 'number');
    assert.ok((result.retry > 0) && (result.retry < 2147483647));

    assert.strictEqual(typeof result.expire, 'number');
    assert.ok((result.expire > 0) && (result.expire < 2147483647));

    assert.strictEqual(typeof result.minttl, 'number');
    assert.ok((result.minttl >= 0) && (result.minttl < 2147483647));

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveSoa_failure(done) {
  const req = dns.resolveSoa('something.invalid', function(err, result) {
    assert.ok(err instanceof Error);
    assert.strictEqual(err.errno, 'ENOTFOUND');

    assert.strictEqual(result, undefined);

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveCname(done) {
  const req = dns.resolveCname('www.microsoft.com', function(err, names) {
    assert.ifError(err);
    assert.ok(names.length > 0);

    for (let i = 0; i < names.length; i++) {
      const name = names[i];
      assert.ok(name);
      assert.strictEqual(typeof name, 'string');
    }

    done();
  });

  checkWrap(req);
});

TEST(function test_resolveCname_failure(done) {
  const req = dns.resolveCname('something.invalid', function(err, result) {
    assert.ok(err instanceof Error);
    assert.strictEqual(err.errno, 'ENOTFOUND');

    assert.strictEqual(result, undefined);

    done();
  });

  checkWrap(req);
});


TEST(function test_resolveTxt(done) {
  const req = dns.resolveTxt('google.com', function(err, records) {
    assert.ifError(err);
    assert.strictEqual(records.length, 1);
    assert.ok(util.isArray(records[0]));
    assert.strictEqual(records[0][0].indexOf('v=spf1'), 0);
    done();
  });

  checkWrap(req);
});

TEST(function test_resolveTxt_failure(done) {
  const req = dns.resolveTxt('something.invalid', function(err, result) {
    assert.ok(err instanceof Error);
    assert.strictEqual(err.errno, 'ENOTFOUND');

    assert.strictEqual(result, undefined);

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_failure(done) {
  const req = dns.lookup('does.not.exist', 4, function(err, ip, family) {
    assert.ok(err instanceof Error);
    assert.strictEqual(err.errno, dns.NOTFOUND);
    assert.strictEqual(err.errno, 'ENOTFOUND');
    assert.ok(!/ENOENT/.test(err.message));
    assert.ok(/does\.not\.exist/.test(err.message));

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_null(done) {
  const req = dns.lookup(null, function(err, ip, family) {
    assert.ifError(err);
    assert.strictEqual(ip, null);
    assert.strictEqual(family, 4);

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_ip_all(done) {
  const req = dns.lookup('127.0.0.1', {all: true}, function(err, ips, family) {
    assert.ifError(err);
    assert.ok(Array.isArray(ips));
    assert.ok(ips.length > 0);
    assert.strictEqual(ips[0].address, '127.0.0.1');
    assert.strictEqual(ips[0].family, 4);

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_null_all(done) {
  const req = dns.lookup(null, {all: true}, function(err, ips, family) {
    assert.ifError(err);
    assert.ok(Array.isArray(ips));
    assert.strictEqual(ips.length, 0);

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_all_mixed(done) {
  const req = dns.lookup('www.google.com', {all: true}, function(err, ips) {
    assert.ifError(err);
    assert.ok(Array.isArray(ips));
    assert.ok(ips.length > 0);

    ips.forEach(function(ip) {
      if (isIPv4(ip.address))
        assert.strictEqual(ip.family, 4);
      else if (isIPv6(ip.address))
        assert.strictEqual(ip.family, 6);
      else
        common.fail('unexpected IP address');
    });

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


TEST(function test_lookup_failure(done) {
  const req = dns.lookup('nosuchhostimsure', function(err) {
    assert(err instanceof Error);
    assert.strictEqual(err.code, 'ENOTFOUND');  // Silly error code...
    assert.strictEqual(err.hostname, 'nosuchhostimsure');
    assert.ok(/nosuchhostimsure/.test(err.message));

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


let getaddrinfoCallbackCalled = false;

console.log('looking up nodejs.org...');

const cares = process.binding('cares_wrap');
const req = new cares.GetAddrInfoReqWrap();
cares.getaddrinfo(req, 'nodejs.org', 4);

req.oncomplete = function(err, domains) {
  assert.strictEqual(err, 0);
  console.log('nodejs.org = ', domains);
  assert.ok(Array.isArray(domains));
  assert.ok(domains.length >= 1);
  assert.strictEqual(typeof domains[0], 'string');
  getaddrinfoCallbackCalled = true;
};

process.on('exit', function() {
  console.log(completed + ' tests completed');
  assert.strictEqual(running, false);
  assert.strictEqual(expected, completed);
  assert.ok(getaddrinfoCallbackCalled);
});
