// Flags: --expose-internals --no-warnings
'use strict';

// Tests a simple QUIC client/server round-trip

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { internalBinding } = require('internal/test/binding');
const {
  constants: {
    NGTCP2_NO_ERROR,
    QUIC_ERROR_APPLICATION,
  }
} = internalBinding('quic');

const qlog = process.env.NODE_QLOG === '1';

const { Buffer } = require('buffer');
const Countdown = require('../common/countdown');
const assert = require('assert');
const {
  createReadStream,
  createWriteStream,
  readFileSync
} = require('fs');
const { pipeline } = require('stream');
const {
  key,
  cert,
  ca,
  debug,
} = require('../common/quic');

const filedata = readFileSync(__filename, { encoding: 'utf8' });

const { createQuicSocket } = require('net');

const kStatelessResetToken =
  Buffer.from('000102030405060708090A0B0C0D0E0F', 'hex');

const unidata = ['I wonder if it worked.', 'test'];
const kServerName = 'agent2';  // Intentionally the wrong servername
const kALPN = 'zzz';  // ALPN can be overriden to whatever we want

const {
  setImmediate: setImmediatePromise
} = require('timers/promises');

const ocspHandler = common.mustCall(async function(type, options) {
  debug(`QuicClientSession received an OCSP ${type}"`);
  switch (type) {
    case 'request':
      const {
        servername,
        certificate,
        issuer,
      } = options;

      assert.strictEqual(servername, kServerName);

      debug('QuicServerSession Certificate: ', certificate);
      debug('QuicServerSession Issuer: ', issuer);

      // Handshake will pause until the Promise resolves
      await setImmediatePromise();

      return Buffer.from('hello');
    case 'response':
      const { data } = options;
      assert.strictEqual(data.toString(), 'hello');
  }
}, 2);

const clientHelloHandler = common.mustCall(
  async (alpn, servername, ciphers) => {
    assert.strictEqual(alpn, kALPN);
    assert.strictEqual(servername, kServerName);
    assert.strictEqual(ciphers.length, 4);
  });

const options = { key, cert, ca, alpn: kALPN, qlog, ocspHandler };

const client = createQuicSocket({ qlog, client: options });
const server = createQuicSocket({
  qlog,
  validateAddress: true,
  statelessResetSecret: kStatelessResetToken,
  server: options
});

const countdown = new Countdown(2, () => {
  debug('Countdown expired. Destroying sockets');
  server.close();
  client.close();
});

function onSocketClose() {
  debug(`${this.constructor.name} closing. Duration`, this.duration);
  debug('  Bound duration:',
        this.boundDuration);
  debug('  Listen duration:',
        this.listenDuration);
  debug('  Bytes Sent/Received: %d/%d',
        this.bytesSent,
        this.bytesReceived);
  debug('  Packets Sent/Received: %d/%d',
        this.packetsSent,
        this.packetsReceived);
  debug('  Sessions:', this.serverSessions, this.clientSessions);
}

server.on('listening', common.mustCall());
server.on('ready', common.mustCall());
server.on('close', common.mustCall(onSocketClose.bind(server)));
client.on('endpointClose', common.mustCall());
client.on('close', common.mustCall(onSocketClose.bind(client)));

(async function() {
  server.on('session', common.mustCall(async (session) => {
    if (qlog) session.qlog.pipe(createWriteStream('server.qlog'));
    debug('QuicServerSession Created');

    assert.strictEqual(session.maxStreams.bidi, 100);
    assert.strictEqual(session.maxStreams.uni, 3);

    {
      const {
        address,
        family,
        port
      } = session.remoteAddress;
      const endpoint = client.endpoints[0].address;
      assert.strictEqual(port, endpoint.port);
      assert.strictEqual(family, endpoint.family);
      debug(`QuicServerSession Client ${family} address ${address}:${port}`);
    }

    session.on('secure', common.mustCall((servername, alpn, cipher) => {
      debug('QuicServerSession TLS Handshake Complete');
      debug('  Server name: %s', servername);
      debug('  ALPN: %s', alpn);
      debug('  Cipher: %s, %s', cipher.name, cipher.version);
      assert.strictEqual(session.servername, servername);
      assert.strictEqual(servername, kServerName);
      assert.strictEqual(session.alpnProtocol, alpn);
      assert.strictEqual(session.getPeerCertificate().subject.CN, 'agent1');
      assert(session.authenticated);
      assert.strictEqual(session.authenticationError, undefined);
    }));

    const uni = await session.openStream({ halfOpen: true });
    debug('Unidirectional, Server-initiated stream %d opened', uni.id);
    assert(uni.writable);
    assert(!uni.readable);
    assert(uni.unidirectional);
    assert(!uni.bidirectional);
    assert(uni.serverInitiated);
    assert(!uni.clientInitiated);
    uni.on('end', common.mustNotCall());
    uni.on('data', common.mustNotCall());
    uni.write(unidata[0], common.mustCall());
    uni.end(unidata[1]);
    // TODO(@jasnell): There's currently a bug where the final
    // write callback is not invoked if the stream/session is
    // destroyed before we receive the acknowledgement for the
    // write.
    // uni.end(unidata[1], common.mustCall());
    // uni.on('finish', common.mustCall());
    uni.on('close', common.mustCall(() => {
      assert.strictEqual(uni.finalSize, 0);
    }));

    session.on('stream', common.mustCall((stream) => {
      debug('Bidirectional, Client-initiated stream %d received', stream.id);
      assert.strictEqual(stream.id, 0);
      assert.strictEqual(stream.session, session);
      assert(stream.writable);
      assert(stream.readable);
      assert(stream.bidirectional);
      assert(!stream.unidirectional);
      assert(stream.clientInitiated);
      assert(!stream.serverInitiated);

      let data = '';
      pipeline(createReadStream(__filename), stream, common.mustCall((err) => {
        assert.ifError(err);
      }));

      stream.setEncoding('utf8');
      stream.on('blocked', common.mustNotCall());
      stream.on('data', (chunk) => {
        data += chunk;

        debug('Server: min data rate: %f', stream.dataRateHistogram.min);
        debug('Server: max data rate: %f', stream.dataRateHistogram.max);
        debug('Server: data rate 50%: %f',
              stream.dataRateHistogram.percentile(50));
        debug('Server: data rate 99%: %f',
              stream.dataRateHistogram.percentile(99));

        debug('Server: min data size: %f', stream.dataSizeHistogram.min);
        debug('Server: max data size: %f', stream.dataSizeHistogram.max);
        debug('Server: data size 50%: %f',
              stream.dataSizeHistogram.percentile(50));
        debug('Server: data size 99%: %f',
              stream.dataSizeHistogram.percentile(99));
      });
      stream.on('end', common.mustCall(() => {
        assert.strictEqual(data, filedata);
        debug('Server received expected data for stream %d', stream.id);
      }));
      stream.on('finish', common.mustCall());
      stream.on('close', common.mustCall(() => {
        assert.strictEqual(typeof stream.duration, 'number');
        assert.strictEqual(typeof stream.bytesReceived, 'number');
        assert.strictEqual(typeof stream.bytesSent, 'number');
        assert.strictEqual(typeof stream.maxExtendedOffset, 'number');
        assert.strictEqual(stream.finalSize, filedata.length);
      }));
    }));

    session.on('close', common.mustCall(() => {
      const {
        code,
        family
      } = session.closeCode;
      debug(`Server session closed with code ${code} (family: ${family})`);
      assert.strictEqual(code, NGTCP2_NO_ERROR);

      const err = {
        code: 'ERR_INVALID_STATE',
        name: 'Error'
      };
      assert.throws(() => session.ping(), err);
      assert.throws(() => session.updateKey(), err);
      assert.rejects(() => session.openStream(), err);
    }));
  }));

  await server.listen({
    requestCert: true,
    rejectUnauthorized: false,
    clientHelloHandler
  });

  const endpoints = server.endpoints;
  for (const endpoint of endpoints) {
    const address = endpoint.address;
    debug('Server is listening on address %s:%d',
          address.address,
          address.port);
  }
  const endpoint = endpoints[0];

  const req = await client.connect({
    address: 'localhost',
    port: endpoint.address.port,
    servername: kServerName,
  });
  if (qlog) req.qlog.pipe(createWriteStream('client.qlog'));

  assert.strictEqual(req.servername, kServerName);

  req.on('usePreferredAddress', common.mustNotCall());

  req.on('sessionTicket', common.mustCall((ticket, params) => {
    debug('Session ticket received');
    assert(ticket instanceof Buffer);
    assert(params instanceof Buffer);
    debug('  Ticket: %s', ticket.toString('hex'));
    debug('  Params: %s', params.toString('hex'));
  }, 2));

  req.on('secure', common.mustCall(async (servername, alpn, cipher) => {
    debug('QuicClientSession TLS Handshake Complete');
    debug('  Server name: %s', servername);
    debug('  ALPN: %s', alpn);
    debug('  Cipher: %s, %s', cipher.name, cipher.version);
    assert.strictEqual(servername, kServerName);
    assert.strictEqual(req.servername, kServerName);
    assert.strictEqual(alpn, kALPN);
    assert.strictEqual(req.alpnProtocol, kALPN);
    assert(req.ephemeralKeyInfo);
    assert.strictEqual(req.getPeerCertificate().subject.CN, 'agent1');

    debug('Client, min handshake ack: %f',
          req.handshakeAckHistogram.min);
    debug('Client, max handshake ack: %f',
          req.handshakeAckHistogram.max);
    debug('Client, min handshake rate: %f',
          req.handshakeContinuationHistogram.min);
    debug('Client, max handshake rate: %f',
          req.handshakeContinuationHistogram.max);

    // The server's identity won't be valid because the requested
    // SNI hostname does not match the certificate used.
    debug('QuicClientSession server is %sauthenticated',
          req.authenticated ? '' : 'not ');
    assert(!req.authenticated);
    assert.throws(() => { throw req.authenticationError; }, {
      code: 'ERR_QUIC_VERIFY_HOSTNAME_MISMATCH',
      message: 'Hostname mismatch'
    });
  }));

  req.on('stream', common.mustCall((stream) => {
    debug('Unidirectional, Server-initiated stream %d received', stream.id);
    let data = '';
    assert(stream.readable);
    assert(!stream.writable);
    stream.setEncoding('utf8');
    stream.on('data', (chunk) => data += chunk);
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(data, unidata.join(''));
      debug('Client received expected data for stream %d', stream.id);
    }));
    stream.on('close', common.mustCall(() => {
      debug('Unidirectional, Server-initiated stream %d closed', stream.id);
      assert.strictEqual(stream.finalSize, 26);
      countdown.dec();
    }));
  }));

  req.on('close', common.mustCall(() => {
    const {
      code,
      family
    } = req.closeCode;
    debug(`Client session closed with code ${code} (family: ${family})`);
    assert.strictEqual(code, NGTCP2_NO_ERROR);
    assert.strictEqual(family, QUIC_ERROR_APPLICATION);
  }));

  {
    const {
      address,
      family,
      port
    } = req.remoteAddress;
    const endpoint = server.endpoints[0].address;
    assert.strictEqual(port, endpoint.port);
    assert.strictEqual(family, endpoint.family);
    debug(`QuicClientSession Server ${family} address ${address}:${port}`);
  }

  const stream = await req.openStream();
  pipeline(createReadStream(__filename), stream, common.mustCall((err) => {
    assert.ifError(err);
  }));
  let data = '';
  stream.resume();
  stream.setEncoding('utf8');
  stream.on('finish', common.mustCall());
  stream.on('blocked', common.mustNotCall());
  stream.on('data', (chunk) => data += chunk);
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(data, filedata);
    debug('Client received expected data for stream %d', stream.id);
  }));
  stream.on('close', common.mustCall(() => {
    debug('Bidirectional, Client-initiated stream %d closed', stream.id);
    assert.strictEqual(stream.finalSize, filedata.length);
    countdown.dec();
  }));
  debug('Bidirectional, Client-initiated stream %d opened', stream.id);
})().then(common.mustCall());
