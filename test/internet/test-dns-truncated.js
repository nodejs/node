'use strict';

var common = require('../common');

if (process.oldDNS)
  process.exit(0);

var assert = require('assert'),
    dns = require('dns'),
    dgram = require('dgram'),
    net = require('net');

var UDPServer = dgram.createSocket('udp4'),
    TCPServer = net.createServer(),
    listening = 0,
    results = [];


dns.setServers(['127.0.0.1']);
UDPServer.bind(53, '127.0.0.1', addTests);
TCPServer.listen(53, '127.0.0.1', addTests);

function addTests() {
  if (++listening < 2)
    return;

  var replybuf = new Buffer([
    0x00, 0x00, // ID
    parseInt([
      '1', // QR
      '0000', // OPCODE
      '1', // AA
      '1', // TC
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
  ]);
  var opts = {
    hints: 0,
    flags: dns.FLAG_RESOLVE_NSONLY,
    family: 4
  };

  UDPServer.on('message', function(buf, rinfo) {
    results.push('udp');
    UDPServer.send(replybuf, 0, replybuf.length, rinfo.port, rinfo.address);
  });

  function readN(s, n, cb) {
    var r = s.read(n);
    if (r === null)
      return s.once('readable', readN.bind(null, s, n, cb));
    cb(r);
  }
  TCPServer.on('connection', function(s) {
    results.push('tcp');
    readN(s, 2, function readLength(buf) {
      readN(s, buf.readUInt16BE(0), function readBytes(buf) {
        var lenbuf = new Buffer(2);
        lenbuf.writeUInt16BE(replybuf.length, 0);
        s.write(lenbuf);
        s.write(replybuf);
      });
    });
  });

  var timer = setTimeout(function() {
    throw new Error('Timeout');
  }, 100);
  dns.lookup('example.org', opts, function(err, rep) {
    clearTimeout(timer);
    UDPServer.close();
    TCPServer.close();
    assert.ok(err && err.code === 'ENOTFOUND');
    assert.ok(rep === undefined);
  });
}


process.on('exit', function() {
  assert.deepEqual(results, ['udp', 'tcp']);
});
