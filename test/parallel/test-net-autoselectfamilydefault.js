'use strict';

const common = require('../common');
const { parseDNSPacket, writeDNSPacket } = require('../common/dns');

const assert = require('assert');
const dgram = require('dgram');
const { Resolver } = require('dns');
const { createConnection, createServer, setDefaultAutoSelectFamily } = require('net');

// Test that the default for happy eyeballs algorithm is properly respected.

let autoSelectFamilyAttemptTimeout = common.platformTimeout(250);
if (common.isWindows) {
  // Some of the windows machines in the CI need more time to establish connection
  autoSelectFamilyAttemptTimeout = common.platformTimeout(1500);
}

function _lookup(resolver, hostname, options, cb) {
  resolver.resolve(hostname, 'ANY', (err, replies) => {
    assert.notStrictEqual(options.family, 4);

    if (err) {
      return cb(err);
    }

    const hosts = replies
      .map((r) => ({ address: r.address, family: r.type === 'AAAA' ? 6 : 4 }))
      .sort((a, b) => b.family - a.family);

    if (options.all === true) {
      return cb(null, hosts);
    }

    return cb(null, hosts[0].address, hosts[0].family);
  });
}

function createDnsServer(ipv6Addr, ipv4Addr, cb) {
  // Create a DNS server which replies with a AAAA and a A record for the same host
  const socket = dgram.createSocket('udp4');

  socket.on('message', common.mustCall((msg, { address, port }) => {
    const parsed = parseDNSPacket(msg);
    const domain = parsed.questions[0].domain;
    assert.strictEqual(domain, 'example.org');

    socket.send(writeDNSPacket({
      id: parsed.id,
      questions: parsed.questions,
      answers: [
        { type: 'AAAA', address: ipv6Addr, ttl: 123, domain: 'example.org' },
        { type: 'A', address: ipv4Addr, ttl: 123, domain: 'example.org' },
      ]
    }), port, address);
  }));

  socket.bind(0, () => {
    const resolver = new Resolver();
    resolver.setServers([`127.0.0.1:${socket.address().port}`]);

    cb({ dnsServer: socket, lookup: _lookup.bind(null, resolver) });
  });
}

// Test that IPV4 is reached by default if IPV6 is not reachable and the default is enabled
{
  createDnsServer('::1', '127.0.0.1', common.mustCall(function({ dnsServer, lookup }) {
    const ipv4Server = createServer((socket) => {
      socket.on('data', common.mustCall(() => {
        socket.write('response-ipv4');
        socket.end();
      }));
    });

    ipv4Server.listen(0, '127.0.0.1', common.mustCall(() => {
      setDefaultAutoSelectFamily(true);

      const connection = createConnection({
        host: 'example.org',
        port: ipv4Server.address().port,
        lookup,
        autoSelectFamilyAttemptTimeout,
      });

      let response = '';
      connection.setEncoding('utf-8');

      connection.on('data', (chunk) => {
        response += chunk;
      });

      connection.on('end', common.mustCall(() => {
        assert.strictEqual(response, 'response-ipv4');
        ipv4Server.close();
        dnsServer.close();
      }));

      connection.write('request');
    }));
  }));
}

// Test that IPV4 is not reached by default if IPV6 is not reachable and the default is disabled
{
  createDnsServer('::1', '127.0.0.1', common.mustCall(function({ dnsServer, lookup }) {
    const ipv4Server = createServer((socket) => {
      socket.on('data', common.mustCall(() => {
        socket.write('response-ipv4');
        socket.end();
      }));
    });

    ipv4Server.listen(0, '127.0.0.1', common.mustCall(() => {
      setDefaultAutoSelectFamily(false);

      const port = ipv4Server.address().port;

      const connection = createConnection({
        host: 'example.org',
        port,
        lookup,
      });

      connection.on('ready', common.mustNotCall());
      connection.on('error', common.mustCall((error) => {
        if (common.hasIPv6) {
          assert.strictEqual(error.code, 'ECONNREFUSED');
          assert.strictEqual(error.message, `connect ECONNREFUSED ::1:${port}`);
        } else if (error.code === 'EAFNOSUPPORT') {
          assert.strictEqual(error.message, `connect EAFNOSUPPORT ::1:${port} - Local (undefined:undefined)`);
        } else if (common.isIBMi) {
          // IBMi returns EUNATCH (ERRNO 42) when IPv6 is disabled
          // keep this errno assertion until EUNATCH is recognized by libuv
          assert.strictEqual(error.errno, -42);
        } else {
          assert.strictEqual(error.code, 'EADDRNOTAVAIL');
          assert.strictEqual(error.message, `connect EADDRNOTAVAIL ::1:${port} - Local (:::0)`);
        }

        ipv4Server.close();
        dnsServer.close();
      }));
    }));
  }));
}
