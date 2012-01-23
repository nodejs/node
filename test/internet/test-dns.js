// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var assert = require('assert'),
    dns = require('dns'),
    net = require('net'),
    isIP = net.isIP,
    isIPv4 = net.isIPv4,
    isIPv6 = net.isIPv6;

var expected = 0,
    completed = 0,
    running = false,
    queue = [];


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


TEST(function test_reverse_bogus(done) {
  var error;

  try {
    var req = dns.reverse('bogus ip', function() {
      assert.ok(false);
    });
  } catch (e) {
    error = e;
  }

  assert.ok(error instanceof Error);
  assert.strictEqual(error.errno, 'ENOTIMP');

  done();
});


TEST(function test_resolveMx(done) {
  var req = dns.resolveMx('gmail.com', function(err, result) {
    if (err) throw err;

    assert.ok(result.length > 0);

    for (var i = 0; i < result.length; i++) {
      var item = result[i];
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


TEST(function test_resolveNs(done) {
  var req = dns.resolveNs('rackspace.com', function(err, names) {
    if (err) throw err;

    assert.ok(names.length > 0);

    for (var i = 0; i < names.length; i++) {
      var name = names[i];
      assert.ok(name);
      assert.ok(typeof name === 'string');
    }

    done();
  });

  checkWrap(req);
});


TEST(function test_resolveSrv(done) {
  var req = dns.resolveSrv('_jabber._tcp.google.com', function(err, result) {
    if (err) throw err;

    assert.ok(result.length > 0);

    for (var i = 0; i < result.length; i++) {
      var item = result[i];
      assert.ok(item);
      assert.ok(typeof item === 'object');

      assert.ok(item.name);
      assert.ok(typeof item.name === 'string');

      assert.ok(typeof item.port === 'number');
      assert.ok(typeof item.priority === 'number');
      assert.ok(typeof item.weight === 'number');
    }

    done();
  });

  checkWrap(req);
});


TEST(function test_resolveCname(done) {
  var req = dns.resolveCname('www.google.com', function(err, names) {
    if (err) throw err;

    assert.ok(names.length > 0);

    for (var i = 0; i < names.length; i++) {
      var name = names[i];
      assert.ok(name);
      assert.ok(typeof name === 'string');
    }

    done();
  });

  checkWrap(req);
});


TEST(function test_resolveTxt(done) {
  var req = dns.resolveTxt('google.com', function(err, records) {
    if (err) throw err;
    assert.equal(records.length, 1);
    assert.equal(records[0].indexOf('v=spf1'), 0);
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


TEST(function test_lookup_ipv6_explicit(done) {
  var req = dns.lookup('ipv6.google.com', 6, function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_ipv6_implicit(done) {
  var req = dns.lookup('ipv6.google.com', function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv6(ip));
    assert.strictEqual(family, 6);

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_failure(done) {
  var req = dns.lookup('does.not.exist', 4, function(err, ip, family) {
    assert.ok(err instanceof Error);
    assert.strictEqual(err.errno, dns.NOTFOUND);
    assert.strictEqual(err.errno, 'ENOTFOUND');

    done();
  });

  checkWrap(req);
});


TEST(function test_lookup_null(done) {
  var req = dns.lookup(null, function(err, ip, family) {
    if (err) throw err;
    assert.strictEqual(ip, null);
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


TEST(function test_lookup_ip_ipv6(done) {
  var req = dns.lookup('::1', function(err, ip, family) {
    if (err) throw err;
    assert.ok(net.isIPv6(ip));
    assert.strictEqual(family, 6);

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


var getaddrinfoCallbackCalled = false;

console.log('looking up nodejs.org...');
var req = process.binding('cares_wrap').getaddrinfo('nodejs.org');

req.oncomplete = function(domains) {
  console.log('nodejs.org = ', domains);
  assert.ok(Array.isArray(domains));
  assert.ok(domains.length >= 1);
  assert.ok(typeof domains[0] == 'string');
  getaddrinfoCallbackCalled = true;
};



process.on('exit', function() {
  console.log(completed + ' tests completed');
  assert.equal(running, false);
  assert.strictEqual(expected, completed);
  assert.ok(getaddrinfoCallbackCalled);
});
