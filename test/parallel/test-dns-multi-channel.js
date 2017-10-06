'use strict';
const common = require('../common');
const dnstools = require('../common/dns');
const { Resolver } = require('dns');
const assert = require('assert');
const dgram = require('dgram');

const servers = [
  {
    socket: dgram.createSocket('udp4'),
    reply: { type: 'A', address: '1.2.3.4', ttl: 123, domain: 'example.org' }
  },
  {
    socket: dgram.createSocket('udp4'),
    reply: { type: 'A', address: '5.6.7.8', ttl: 123, domain: 'example.org' }
  }
];

let waiting = servers.length;
for (const { socket, reply } of servers) {
  socket.on('message', common.mustCall((msg, { address, port }) => {
    const parsed = dnstools.parseDNSPacket(msg);
    const domain = parsed.questions[0].domain;
    assert.strictEqual(domain, 'example.org');

    socket.send(dnstools.writeDNSPacket({
      id: parsed.id,
      questions: parsed.questions,
      answers: [reply],
    }), port, address);
  }));

  socket.bind(0, common.mustCall(() => {
    if (0 === --waiting) ready();
  }));
}


function ready() {
  const resolvers = servers.map((server) => ({
    server,
    resolver: new Resolver()
  }));

  for (const { server: { socket, reply }, resolver } of resolvers) {
    resolver.setServers([`127.0.0.1:${socket.address().port}`]);
    resolver.resolve4('example.org', common.mustCall((err, res) => {
      assert.ifError(err);
      assert.deepStrictEqual(res, [reply.address]);
      socket.close();
    }));
  }
}
