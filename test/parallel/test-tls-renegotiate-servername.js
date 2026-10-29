'use strict';

// A connection's secure context - its certificate, and the CAs it checks client
// certificates against - is chosen from the server name of its first client
// hello, but a connection can do multiple hellos: a TLS 1.2 renegotiation, or
// a TLS 1.3 HelloRetryRequest. These later client hellos must not be allowed
// to subsequently change the server name, since it can associate state and
// authorization (mTLS) from one SNI with another, unexpectedly.
//
// Node's own client can't change the name mid-connection, so the client here
// drives the TLS library directly over memory BIOs, leaving the socket I/O to
// node:net.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

common.skipIfFFIMissing();

if (common.isWindows)
  common.skip('dlopen(null) is not supported on Windows');

// SSL_ctrl() and SSL_CTX_ctrl() take and return a C `long`, which node:ffi has
// no type for, so they're declared below as i64, which it only is on 64-bit.
if (['ia32', 'arm'].includes(process.arch))
  common.skip('a C long is not 64 bits here');

// The SSL_ctrl() command numbers below are OpenSSL's, and BoringSSL neither
// renegotiates nor reads the server name of a second ClientHello.
// BoringSSL guards against the whole class of errors internally anyway, and
// the FFI implementation for testing below is OpenSSL specific:
if (require('../common/crypto').isBoringSSL)
  common.skip('OpenSSL-specific');

const assert = require('node:assert');
const net = require('node:net');
const tls = require('node:tls');
const fixtures = require('../common/fixtures');
const { dlopen } = require('node:ffi');

const SSL_CTRL_SET_TLSEXT_HOSTNAME = 55;
const SSL_CTRL_SET_MAX_PROTO_VERSION = 124;
const TLSEXT_NAMETYPE_host_name = 0n;
const TLS1_2_VERSION = 0x0303n;
const SSL_ERROR_WANT_READ = 2;
const SSL_ERROR_WANT_WRITE = 3;

// Node's own OpenSSL, resolved out of the running process rather than loaded
// from the system.
const { functions: ssl } = dlopen(null, {
  TLS_client_method: { arguments: [], return: 'pointer' },
  SSL_CTX_new: { arguments: ['pointer'], return: 'pointer' },
  SSL_CTX_free: { arguments: ['pointer'], return: 'void' },
  SSL_CTX_ctrl: { arguments: ['pointer', 'i32', 'i64', 'pointer'], return: 'i64' },
  SSL_new: { arguments: ['pointer'], return: 'pointer' },
  SSL_free: { arguments: ['pointer'], return: 'void' },
  SSL_ctrl: { arguments: ['pointer', 'i32', 'i64', 'string'], return: 'i64' },
  SSL_set_bio: { arguments: ['pointer', 'pointer', 'pointer'], return: 'void' },
  SSL_set_connect_state: { arguments: ['pointer'], return: 'void' },
  SSL_do_handshake: { arguments: ['pointer'], return: 'i32' },
  SSL_renegotiate: { arguments: ['pointer'], return: 'i32' },
  SSL_get_error: { arguments: ['pointer', 'i32'], return: 'i32' },
  SSL_get1_session: { arguments: ['pointer'], return: 'pointer' },
  SSL_set_session: { arguments: ['pointer', 'pointer'], return: 'i32' },
  SSL_session_reused: { arguments: ['pointer'], return: 'i32' },
  SSL_SESSION_free: { arguments: ['pointer'], return: 'void' },
  BIO_s_mem: { arguments: [], return: 'pointer' },
  BIO_new: { arguments: ['pointer'], return: 'pointer' },
  BIO_read: { arguments: ['pointer', 'buffer', 'i32'], return: 'i32' },
  BIO_write: { arguments: ['pointer', 'buffer', 'i32'], return: 'i32' },
});

// A client whose records are carried by a plain socket. The server name is
// settable at any point, which is the whole reason for this class.
class RawClient {
  constructor({ tls12Only = false, session } = {}) {
    this.ctx = ssl.SSL_CTX_new(ssl.TLS_client_method());
    if (tls12Only) {
      ssl.SSL_CTX_ctrl(this.ctx, SSL_CTRL_SET_MAX_PROTO_VERSION, TLS1_2_VERSION, null);
    }
    this.ssl = ssl.SSL_new(this.ctx);
    if (session) ssl.SSL_set_session(this.ssl, session);
    this.rbio = ssl.BIO_new(ssl.BIO_s_mem());
    this.wbio = ssl.BIO_new(ssl.BIO_s_mem());
    // SSL_set_bio() takes ownership of both, so SSL_free() releases them.
    ssl.SSL_set_bio(this.ssl, this.rbio, this.wbio);
    ssl.SSL_set_connect_state(this.ssl);

    this.buf = Buffer.alloc(16 * 1024);
    this.incoming = [];
    this.notify = null;
    this.ended = false;
    this.socket = net.connect(server.address().port, '127.0.0.1');
    this.socket.on('data', (chunk) => {
      this.incoming.push(chunk);
      this.wake();
    });
    this.socket.on('close', () => {
      this.ended = true;
      this.wake();
    });
    this.socket.on('error', () => {});
    this.connected = new Promise((resolve) => this.socket.once('connect', resolve));
  }

  wake() {
    const notify = this.notify;
    this.notify = null;
    notify?.();
  }

  // A null name clears it, so the next ClientHello carries no extension.
  setServername(name) {
    ssl.SSL_ctrl(this.ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name, name);
  }

  // Everything the TLS library has produced goes out on the socket.
  flush() {
    for (;;) {
      const read = ssl.BIO_read(this.wbio, this.buf, this.buf.length);
      if (read <= 0) break;
      this.socket.write(Buffer.from(this.buf.subarray(0, read)));
    }
  }

  // Hands the library the next thing to arrive, false once nothing more will.
  async feed() {
    while (this.incoming.length === 0) {
      if (this.ended) return false;
      await new Promise((resolve) => { this.notify = resolve; });
    }
    const chunk = this.incoming.shift();
    ssl.BIO_write(this.rbio, chunk, chunk.length);
    return true;
  }

  // `afterFirstRead` runs once the library has been handed the server's first
  // flight but before it has written anything in reply. For a TLS 1.3
  // HelloRetryRequest that is the gap between the two ClientHellos.
  async handshake(afterFirstRead) {
    let reads = 0;
    for (;;) {
      const result = ssl.SSL_do_handshake(this.ssl);
      this.flush();
      if (result === 1) return true;
      const error = ssl.SSL_get_error(this.ssl, result);
      if (error !== SSL_ERROR_WANT_READ && error !== SSL_ERROR_WANT_WRITE) return false;
      if (!await this.feed()) return false;
      if (++reads === 1) afterFirstRead?.();
    }
  }

  async renegotiate() {
    if (ssl.SSL_renegotiate(this.ssl) !== 1) return false;
    return this.handshake();
  }

  close() {
    this.socket.destroy();
    ssl.SSL_free(this.ssl);
    ssl.SSL_CTX_free(this.ctx);
  }
}

const server = tls.createServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
  // Offering only this group makes the server answer a TLS 1.3 client, whose
  // first key share is for a different curve, with a HelloRetryRequest.
  ecdhCurve: 'P-521',
}, (socket) => {
  socket.on('error', () => {});
  socket.resume();
});
server.on('tlsClientError', () => {});

// Do a set of TLS 1.2 handshakes, asking for SNI `first` (or null) then
// renegotiating for SNI `second` (or null), and returning whether that
// was accepted. If `resume` is set, the `first` session is a resumption.
async function renegotiating(first, second, { resume = false } = {}) {
  let session = null;
  let issuer = null;
  if (resume) {
    // The session must be used before the client it came from is closed:
    // freeing that client's context makes its sessions unresumable.
    issuer = new RawClient({ tls12Only: true });
    await issuer.connected;
    issuer.setServername(first);
    assert.strictEqual(await issuer.handshake(), true);
    session = ssl.SSL_get1_session(issuer.ssl);
  }
  const client = new RawClient({ tls12Only: true, session });
  await client.connected;
  client.setServername(first);
  assert.strictEqual(await client.handshake(), true);
  assert.strictEqual(ssl.SSL_session_reused(client.ssl) === 1, resume);
  client.setServername(second);
  const accepted = await client.renegotiate();
  client.close();
  if (resume) {
    issuer.close();
    ssl.SSL_SESSION_free(session);
  }
  return accepted;
}

// One TLS 1.3 handshake, asking for `first` in the ClientHello and `second` in
// the one that answers the HelloRetryRequest, returning whether it succeeded.
async function retrying(first, second) {
  const client = new RawClient();
  await client.connected;
  client.setServername(first);
  const accepted = await client.handshake(() => client.setServername(second));
  client.close();
  return accepted;
}

server.listen(0, common.mustCall(async () => {
  // Renegotiating must ask for exactly the name the first handshake did, or
  // none if that had none. Changing, dropping or adding a name is refused.
  for (const resume of [false, true]) {
    for (const [first, second, accepted] of [
      ['agent1.example', 'agent1.example', true],
      [null, null, true],
      ['agent1.example', 'agent3.example', false],
      ['agent1.example', null, false],
      [null, 'agent1.example', false],
    ]) {
      assert.strictEqual(await renegotiating(first, second, { resume }),
                         accepted,
                         `${resume ? 'resuming then ' : ''}renegotiating ` +
                         `from ${first} to ${second}`);
    }
  }

  // TLS1.3 HelloRetryRequest can't change SNI either:
  assert.strictEqual(await retrying('agent1.example', 'agent1.example'), true);
  assert.strictEqual(await retrying('agent1.example', 'agent3.example'), false);

  server.close();
}));
