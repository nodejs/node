'use strict';

const common = require('../common');
const { parseDNSPacket, writeDNSPacket } = require('../common/dns');

const assert = require('assert');
const dgram = require('dgram');
const { Resolver } = require('dns');
const { createConnection, createServer } = require('net');

// Test that happy eyeballs algorithm is properly implemented.

// Purposely not using setDefaultAutoSelectFamilyAttemptTimeout here to test the
// parameter is correctly used in options.
//
// Some of the machines in the CI need more time to establish connection
const autoSelectFamilyAttemptTimeout = common.defaultAutoSelectFamilyAttemptTimeout;

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

function createDnsServer(ipv6Addrs, ipv4Addrs, cb) {
  if (!Array.isArray(ipv6Addrs)) {
    ipv6Addrs = [ipv6Addrs];
  }

  if (!Array.isArray(ipv4Addrs)) {
    ipv4Addrs = [ipv4Addrs];
  }

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
        ...ipv6Addrs.map((address) => ({ type: 'AAAA', address, ttl: 123, domain: 'example.org' })),
        ...ipv4Addrs.map((address) => ({ type: 'A', address, ttl: 123, domain: 'example.org' })),
      ]
    }), port, address);
  }));

  socket.bind(0, () => {
    const resolver = new Resolver();
    resolver.setServers([`127.0.0.1:${socket.address().port}`]);

    cb({ dnsServer: socket, lookup: _lookup.bind(null, resolver) });
  });
}

// Test that IPV4 is reached if IPV6 is not reachable
{
  createDnsServer('::1', '127.0.0.1', common.mustCall(function({ dnsServer, lookup }) {
    const ipv4Server = createServer((socket) => {
      socket.on('data', common.mustCall(() => {
        socket.write('response-ipv4');
        socket.end();
      }));
    });

    ipv4Server.listen(0, '127.0.0.1', common.mustCall(() => {
      const port = ipv4Server.address().port;

      const connection = createConnection({
        host: 'example.org',
        port: port,
        lookup,
        autoSelectFamily: true,
        autoSelectFamilyAttemptTimeout,
      });

      let response = '';
      connection.setEncoding('utf-8');

      connection.on('ready', common.mustCall(() => {
        assert.deepStrictEqual(connection.autoSelectFamilyAttemptedAddresses, [`::1:${port}`, `127.0.0.1:${port}`]);
      }));

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

// Test that only the last successful connection is established.
{
  createDnsServer(
    ['2606:4700::6810:85e5', '2606:4700::6810:84e5', '::1'],
    ['104.20.22.46', '104.20.23.46', '127.0.0.1'],
    common.mustCall(function({ dnsServer, lookup }) {
      const ipv4Server = createServer((socket) => {
        socket.on('data', common.mustCall(() => {
          socket.write('response-ipv4');
          socket.end();
        }));
      });

      ipv4Server.listen(0, '127.0.0.1', common.mustCall(() => {
        const port = ipv4Server.address().port;

        const connection = createConnection({
          host: 'example.org',
          port: port,
          lookup,
          autoSelectFamily: true,
          autoSelectFamilyAttemptTimeout,
        });

        let response = '';
        connection.setEncoding('utf-8');

        connection.on('ready', common.mustCall(() => {
          assert.deepStrictEqual(
            connection.autoSelectFamilyAttemptedAddresses,
            [
              `2606:4700::6810:85e5:${port}`,
              `104.20.22.46:${port}`,
              `2606:4700::6810:84e5:${port}`,
              `104.20.23.46:${port}`,
              `::1:${port}`,
              `127.0.0.1:${port}`,
            ]
          );
        }));

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
    })
  );
}

// Test that IPV4 is NOT reached if IPV6 is reachable
if (common.hasIPv6) {
  createDnsServer('::1', '127.0.0.1', common.mustCall(function({ dnsServer, lookup }) {
    const ipv4Server = createServer((socket) => {
      socket.on('data', common.mustNotCall(() => {
        socket.write('response-ipv4');
        socket.end();
      }));
    });

    const ipv6Server = createServer((socket) => {
      socket.on('data', common.mustCall(() => {
        socket.write('response-ipv6');
        socket.end();
      }));
    });

    ipv4Server.listen(0, '127.0.0.1', common.mustCall(() => {
      const port = ipv4Server.address().port;

      ipv6Server.listen(port, '::1', common.mustCall(() => {
        const connection = createConnection({
          host: 'example.org',
          port,
          lookup,
          autoSelectFamily: true,
          autoSelectFamilyAttemptTimeout,
        });

        let response = '';
        connection.setEncoding('utf-8');

        connection.on('ready', common.mustCall(() => {
          assert.deepStrictEqual(connection.autoSelectFamilyAttemptedAddresses, [`::1:${port}`]);
        }));

        connection.on('data', (chunk) => {
          response += chunk;
        });

        connection.on('end', common.mustCall(() => {
          assert.strictEqual(response, 'response-ipv6');
          ipv4Server.close();
          ipv6Server.close();
          dnsServer.close();
        }));

        connection.write('request');
      }));
    }));
  }));
}

// Test that when all errors are returned when no connections succeeded
{
  createDnsServer('::1', '127.0.0.1', common.mustCall(function({ dnsServer, lookup }) {
    const connection = createConnection({
      host: 'example.org',
      port: 10,
      lookup,
      autoSelectFamily: true,
      autoSelectFamilyAttemptTimeout,
    });

    connection.on('ready', common.mustNotCall());
    connection.on('error', common.mustCall((error) => {
      assert.deepStrictEqual(connection.autoSelectFamilyAttemptedAddresses, ['::1:10', '127.0.0.1:10']);
      assert.strictEqual(error.constructor.name, 'AggregateError');
      assert.strictEqual(error.errors.length, 2);

      const errors = error.errors.map((e) => e.message);
      assert.ok(errors.includes('connect ECONNREFUSED 127.0.0.1:10'));

      if (common.hasIPv6) {
        assert.ok(errors.includes('connect ECONNREFUSED ::1:10'));
      }

      dnsServer.close();
    }));
  }));
}

// Test that the option can be disabled
{
  createDnsServer('::1', '127.0.0.1', common.mustCall(function({ dnsServer, lookup }) {
    const ipv4Server = createServer((socket) => {
      socket.on('data', common.mustCall(() => {
        socket.write('response-ipv4');
        socket.end();
      }));
    });

    ipv4Server.listen(0, '127.0.0.1', common.mustCall(() => {
      const port = ipv4Server.address().port;

      const connection = createConnection({
        host: 'example.org',
        port,
        lookup,
        autoSelectFamily: false,
      });

      connection.on('ready', common.mustNotCall());
      connection.on('error', common.mustCall((error) => {
        assert.strictEqual(connection.autoSelectFamilyAttemptedAddresses, undefined);

        if (common.hasIPv6) {
          assert.strictEqual(error.code, 'ECONNREFUSED');
          assert.strictEqual(error.message, `connect ECONNREFUSED ::1:${port}`);
        } else if (error.code === 'EAFNOSUPPORT') {
          assert.strictEqual(error.message, `connect EAFNOSUPPORT ::1:${port} - Local (undefined:undefined)`);
        } else if (error.code === 'EUNATCH') {
          assert.strictEqual(error.message, `connect EUNATCH ::1:${port} - Local (:::0)`);
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
