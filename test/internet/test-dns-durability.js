'use strict';

if (process.oldDNS)
  process.exit(0);

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

function UDPServerReply(replybuf, expbuf) {
  var testName = current.name;
  var timer = setTimeout(function() {
    throw new Error(format('Timeout on UDP test (%s)', testName));
  }, 100);
  UDPServer.once('message', function(buf, rinfo) {
    clearTimeout(timer);
    if (expbuf)
      assert.deepEqual(buf, expbuf);
    UDPServer.send(replybuf, 0, replybuf.length, rinfo.port, rinfo.address);
  });
}

function TCPServerReply(replybuf, expbuf) {
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
  TCPServer.once('connection', function(s) {
    readN(s, 2, function readLength(buf) {
      readN(s, buf.readUInt16BE(0), function readBytes(buf) {
        clearTimeout(timer);
        if (expbuf)
          assert.deepEqual(buf, expbuf);
        var lenbuf = new Buffer(2);
        lenbuf.writeUInt16BE(replybuf.length, 0);
        s.write(lenbuf);
        s.write(replybuf);
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
      flags: dns.FLAG_RESOLVE_NSONLY | dns.FLAG_RESOLVE_NOSEARCH |
             (isTCP ? dns.FLAG_RESOLVE_USEVC : 0),
      family: 4
    };

    function customOpts(type) {
      return {
        hints: 0,
        flags: dns.FLAG_RESOLVE_NSONLY | dns.FLAG_RESOLVE_NOSEARCH |
               (isTCP ? dns.FLAG_RESOLVE_USEVC : 0),
        type: type,
        family: (type === 'AAAA' ? 6 : 4)
      };
    }

    TEST(function(done) {
      console.log('%s tests', connType);
      console.log('=========');
      process.nextTick(done);
    });


    TEST(function test_shortheader(done) {
      serverReply(new Buffer([
        0x00
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badid(done) {
      serverReply(new Buffer([
        0x00, 0x05, // ID
        parseInt([
          '0', // QR
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
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badqr(done) {
      serverReply(new Buffer([
        0x00, 0x00, // ID
        parseInt([
          '0', // QR
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
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badopcode(done) {
      serverReply(new Buffer([
        0x00, 0x00, // ID
        parseInt([
          '1', // QR
          '1111', // OPCODE
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
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badqnamelen(done) {
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
        0x00, 0x01, // Question records count
        0x00, 0x00, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x07, // RR name
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badansnamelen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x07, // RR name
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badnsnamelen(done) {
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
        0x00, 0x01, // Authority records count
        0x00, 0x00, // Additional records count
        0x07, // RR name
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badaddlnamelen(done) {
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
        0x00, 0x01, // Additional records count
        0x07, // RR name
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badrrlen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x01, // RR type
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badrrclass(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x01, // RR type
        0x00, 0x00, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x00, // RR rdata length
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badrrrdlen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x01, // RR type
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x10, // RR rdata length
        0x0A, 0x01
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badrrtype(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0xFF, 0xFF, // RR type
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x10, // RR rdata length
        0x0A, 0x01
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badA_addrlen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x01, // RR type (A)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x02, // RR rdata length
        0x0A, 0x01, // ADDRESS
      ]));
      dns.lookup('example.org', opts, function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badNS_namelen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x02, // RR type (NS)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x02, // RR rdata length
        0x0C, 0x61, // NAME
      ]));
      dns.lookup('example.org', customOpts('NS'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badCNAME_namelen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x05, // RR type (CNAME)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x02, // RR rdata length
        0x0C, 0x61, // NAME
      ]));
      dns.lookup('example.org', customOpts('NS'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSOA_mnamelen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x06, // RR type (SOA)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x02, // RR rdata length
        0x0C, 0x61, // NAME
      ]));
      dns.lookup('example.org', customOpts('SOA'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSOA_rnamelen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x06, // RR type (SOA)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x03, // RR rdata length
        0x00, // MNAME
        0x0C, 0x61, // RNAME
      ]));
      dns.lookup('example.org', customOpts('SOA'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSOA_seriallen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x06, // RR type (SOA)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x03, // RR rdata length
        0x00, // MNAME
        0x00, // RNAME
        0x00, // SERIAL
      ]));
      dns.lookup('example.org', customOpts('SOA'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSOA_refreshlen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x06, // RR type (SOA)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x07, // RR rdata length
        0x00, // MNAME
        0x00, // RNAME
        0x00, 0x00, 0x00, 0x00, // SERIAL
        0x00, // REFRESH
      ]));
      dns.lookup('example.org', customOpts('SOA'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSOA_retrylen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x06, // RR type (SOA)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x0B, // RR rdata length
        0x00, // MNAME
        0x00, // RNAME
        0x00, 0x00, 0x00, 0x00, // SERIAL
        0x00, 0x00, 0x00, 0x00, // REFRESH
        0x00, // RETRY
      ]));
      dns.lookup('example.org', customOpts('SOA'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSOA_expirelen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x06, // RR type (SOA)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x0F, // RR rdata length
        0x00, // MNAME
        0x00, // RNAME
        0x00, 0x00, 0x00, 0x00, // SERIAL
        0x00, 0x00, 0x00, 0x00, // REFRESH
        0x00, 0x00, 0x00, 0x00, // RETRY
        0x00, // EXPIRE
      ]));
      dns.lookup('example.org', customOpts('SOA'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSOA_expirelen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x06, // RR type (SOA)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x13, // RR rdata length
        0x00, // MNAME
        0x00, // RNAME
        0x00, 0x00, 0x00, 0x00, // SERIAL
        0x00, 0x00, 0x00, 0x00, // REFRESH
        0x00, 0x00, 0x00, 0x00, // RETRY
        0x00, 0x00, 0x00, 0x00, // EXPIRE
        0x00, // MINIMUM
      ]));
      dns.lookup('example.org', customOpts('SOA'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badPTR_namelen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x0C, // RR type (PTR)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x01, // RR rdata length
        0x04, // NAME
      ]));
      dns.lookup('8.8.8.8', customOpts('PTR'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badMX_preflen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x0F, // RR type (MX)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x01, // RR rdata length
        0x00, // PREFERENCE
      ]));
      dns.lookup('example.org', customOpts('MX'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badMX_exchglen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x0F, // RR type (MX)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x03, // RR rdata length
        0x00, 0x01, // PREFERENCE
        0x07, // EXCHANGE
      ]));
      dns.lookup('example.org', customOpts('MX'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badTXT_stringlen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x10, // RR type (TXT)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x01, // RR rdata length
        0x05, // STRING
      ]));
      dns.lookup('example.org', customOpts('TXT'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badAAAA_addrlen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x1C, // RR type (AAAA)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x02, // RR rdata length
        0xFE, 0x80, // address
      ]));
      dns.lookup('example.org', customOpts('AAAA'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badLOC_len(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x1D, // RR type (LOC)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x01, // RR rdata length
        0x00, // VERSION
      ]));
      dns.lookup('example.org', customOpts('LOC'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badLOC_ver(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x1D, // RR type (LOC)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x10, // RR rdata length
        0x05, // VERSION
        0x00, // SIZE
        0x00, // HORIZONTAL PRECISION
        0x00, // VERTICAL PRECISION
        0x00, 0x00, 0x00, 0x00, // LATITUDE
        0x00, 0x00, 0x00, 0x00, // LONGITUDE
        0x00, 0x00, 0x00, 0x00, // ALTITUDE
      ]));
      dns.lookup('example.org', customOpts('LOC'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSRV_len(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x21, // RR type (SRV)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x01, // RR rdata length
        0x00, // PRIORITY
      ]));
      dns.lookup('example.org', customOpts('SRV'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSRV_namelen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x21, // RR type (SRV)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x07, // RR rdata length
        0x00, 0x01, // PRIORITY
        0x00, 0x01, // WEIGHT
        0x00, 0x16, // PORT
        0x07, // NAME
      ]));
      dns.lookup('example.org', customOpts('SRV'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badNAPTR_len(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x23, // RR type (NAPTR)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x01, // RR rdata length
        0x00, // ORDER
      ]));
      dns.lookup('example.org', customOpts('NAPTR'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badNAPTR_flagslen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x23, // RR type (NAPTR)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x05, // RR rdata length
        0x00, 0x01, // ORDER
        0x00, 0x01, // PREFERENCE
        0x07, // FLAGS STRING
      ]));
      dns.lookup('example.org', customOpts('NAPTR'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badNAPTR_flagslen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x23, // RR type (NAPTR)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x06, // RR rdata length
        0x00, 0x01, // ORDER
        0x00, 0x01, // PREFERENCE
        0x00, // FLAGS STRING
        0x07, // SERVICE STRING
      ]));
      dns.lookup('example.org', customOpts('NAPTR'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badNAPTR_flagslen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x23, // RR type (NAPTR)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x07, // RR rdata length
        0x00, 0x01, // ORDER
        0x00, 0x01, // PREFERENCE
        0x00, // FLAGS STRING
        0x00, // SERVICE STRING
        0x07, // REGEXP STRING
      ]));
      dns.lookup('example.org', customOpts('NAPTR'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badNAPTR_flagslen(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x23, // RR type (NAPTR)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x08, // RR rdata length
        0x00, 0x01, // ORDER
        0x00, 0x01, // PREFERENCE
        0x00, // FLAGS STRING
        0x00, // SERVICE STRING
        0x00, // REGEXP STRING
        0x07, // REPLACEMENT STRING
      ]));
      dns.lookup('example.org', customOpts('NAPTR'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSSHFP_len(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x23, // RR type (NAPTR)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x01, // RR rdata length
        0x01, // ALGORITHM
      ]));
      dns.lookup('example.org', customOpts('SSHFP'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSSHFP_sha1len(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x23, // RR type (NAPTR)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x02, // RR rdata length
        0x01, // ALGORITHM
        0x01, // FINGERPRINT TYPE (SHA-1)
      ]));
      dns.lookup('example.org', customOpts('SSHFP'), function(err, rep) {
        assert.ok(err && err.code === 'ENOTFOUND');
        assert.ok(rep === undefined);
        done();
      });
    });


    TEST(function test_badSSHFP_sha256len(done) {
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
        0x00, 0x01, // Answer records count
        0x00, 0x00, // Authority records count
        0x00, 0x00, // Additional records count
        0x00, // RR name
        0x00, 0x23, // RR type (NAPTR)
        0x00, 0x01, // RR class
        0x00, 0x00, 0x00, 0x00, // RR ttl
        0x00, 0x02, // RR rdata length
        0x01, // ALGORITHM
        0x02, // FINGERPRINT TYPE (SHA-256)
      ]));
      dns.lookup('example.org', customOpts('SSHFP'), function(err, rep) {
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
