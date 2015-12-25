'use strict';

var common = require('../common');

if (process.oldDNS || process.platform === 'win32')
  process.exit(0);

process.env.LOCALDOMAIN = 'example.com example.net example.org';

var assert = require('assert'),
    dns = require('dns'),
    dgram = require('dgram'),
    net = require('net'),
    format = require('util').format;

var UDPServer = dgram.createSocket('udp4'),
    TCPServer = net.createServer(),
    expected = 0,
    completed = 0,
    current = null,
    running = false,
    queue = [],
    listening = 0;


function TEST(f) {
  function next() {
    current = queue.shift();
    if (current) {
      running = true;
      if (current.name)
        console.log(current.name);
      current(done);
    } else {
      try {
        UDPServer.close();
      } catch (ex) {}
      try {
        TCPServer.close();
      } catch (ex) {}
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

function UDPServerReply(replybuf, expbuf, nreqs) {
  if (typeof expbuf === 'number') {
    nreqs = expbuf;
    expbuf = undefined;
  } else if (nreqs === undefined) {
    nreqs = 1;
  }
  var testName = current.name;
  var timer = setTimeout(function() {
    throw new Error(format('Timeout on UDP test (%s)', testName));
  }, 100);
  UDPServer.removeAllListeners('message');
  // Listen for multiple connections in case client retries request
  UDPServer.on('message', function(buf, rinfo) {
    clearTimeout(timer);
    if (expbuf) {
      if (Array.isArray(expbuf)) {
        assert.deepEqual(buf, expbuf.shift());
        if (expbuf.length === 1)
          expbuf = expbuf[0];
        else if (!expbuf.length)
          expbuf = undefined;
      } else {
        assert.deepEqual(buf, expbuf);
        expbuf = undefined;
      }
    }
    UDPServer.send(replybuf, 0, replybuf.length, rinfo.port, rinfo.address);
    if (--nreqs === 0)
      UDPServer.removeAllListeners('message');
  });
}

function TCPServerReply(replybuf, expbuf, nreqs) {
  if (typeof expbuf === 'number') {
    nreqs = expbuf;
    expbuf = undefined;
  } else if (nreqs === undefined) {
    nreqs = 1;
  }
  var testName = current.name;
  var timer = setTimeout(function() {
    throw new Error(format('Timeout on TCP test (%s)', testName));
  }, 100);
  function readN(s, n, cb) {
    var r = s.read(n);
    if (r === null)
      return s.once('readable', readN.bind(null, s, n, cb));
    cb(r);
  }
  TCPServer.removeAllListeners('connection');
  // Listen for multiple connections in case client retries request
  TCPServer.on('connection', function(s) {
    readN(s, 2, function readLength(buf) {
      readN(s, buf.readUInt16BE(0), function readBytes(buf) {
        clearTimeout(timer);
        if (expbuf) {
          if (Array.isArray(expbuf)) {
            assert.deepEqual(buf, expbuf.shift());
            if (expbuf.length === 1)
              expbuf = expbuf[0];
            else if (!expbuf.length)
              expbuf = undefined;
          } else {
            assert.deepEqual(buf, expbuf);
            expbuf = undefined;
          }
        }
        var lenbuf = new Buffer(2);
        lenbuf.writeUInt16BE(replybuf.length, 0);
        s.write(lenbuf);
        s.write(replybuf);
        if (--nreqs === 0)
          TCPServer.removeAllListeners('connection');
      });
    });
  });
}

dns.setServers(['127.0.0.1']);
UDPServer.bind(53, '127.0.0.1', addTests);
TCPServer.listen(53, '127.0.0.1', addTests);

function addTests() {
  if (++listening < 2)
    return;

  ['UDP', 'TCP'].forEach(function(connType) {
    var isTCP = (connType === 'TCP');
    var serverReply = (isTCP ? TCPServerReply : UDPServerReply);
    var opts = {
      hints: 0,
      flags: dns.FLAG_RESOLVE_NSONLY | (isTCP ? dns.FLAG_RESOLVE_USEVC : 0),
      family: 4
    };

    TEST(function(done) {
      console.log('%s tests', connType);
      console.log('=========');
      process.nextTick(done);
    });


    TEST(function test_search(done) {
      // Should be 4 attempts, 1 for each domain in LOCALDOMAIN and then as-is
      serverReply(new Buffer([
        0x00, 0x00, // ID
        parseInt([
          '1', // QR
          '0000', // OPCODE
          '1', // AA
          '0', // TC
          '1', // RD
        ].join(''), 2),
        parseInt([
          '1', // RA
          '000', // Z
          '0000', // RCODE
        ].join(''), 2),
        0x00, 0x00, // Question records count
        0x00, 0x00, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
      ]), 4);
      dns.lookup('foo', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });
  });
}


process.on('exit', function() {
  if (expected > 0)
    console.log('%d/%d tests completed', completed, expected);
  assert.equal(running, false);
  assert.strictEqual(expected, completed);
});
