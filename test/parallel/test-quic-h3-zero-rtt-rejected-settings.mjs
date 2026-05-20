// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: H3 0-RTT rejected when server reduces application settings.
// 0-RTT rejected when max_field_section_size decreased
// 0-RTT rejected when enable_connect_protocol disabled
// 0-RTT rejected when enable_datagrams disabled
// Each test creates two endpoints with the same key/cert/tokenSecret.
// The first endpoint issues a ticket with generous H3 settings. The
// second endpoint has reduced settings, causing the H3 session ticket
// app data validation to reject 0-RTT.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { ok, strictEqual, rejects } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey, randomBytes } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const sni = { '*': { keys: [key], certs: [cert] } };
const decoder = new TextDecoder();

// Helper: establish an H3 session, get a ticket, close.
async function getTicket(endpointOptions) {
  let savedTicket;
  let savedToken;
  const gotTicket = Promise.withResolvers();
  const gotToken = Promise.withResolvers();

  const ep = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      await stream.closed;
      ss.close();
    });
  }), {
    sni,
    ...endpointOptions,
    onheaders: mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync('ok');
      this.writer.endSync();
    }),
  });

  const cs = await connect(ep.address, {
    servername: 'localhost',
    ...endpointOptions,
    onsessionticket(ticket) {
      ok(Buffer.isBuffer(ticket));
      savedTicket = ticket;
      gotTicket.resolve();
    },
    onnewtoken(token) {
      ok(Buffer.isBuffer(token));
      savedToken = token;
      gotToken.resolve();
    },
  });
  await cs.opened;
  await Promise.all([gotTicket.promise, gotToken.promise]);

  const s = await cs.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/ticket',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });
  const body = await bytes(s);
  strictEqual(decoder.decode(body), 'ok');
  await Promise.all([s.closed, cs.closed]);
  await ep.close();

  return { ticket: savedTicket, token: savedToken };
}

// Helper: attempt 0-RTT with reduced settings, expect rejection.
// When 0-RTT is rejected, the H3 application is torn down and
// recreated (EarlyDataRejected destroys the nghttp3 connection).
// The initial 0-RTT stream may not survive this transition, so we
// only verify earlyDataAccepted is false and close cleanly.
async function attemptRejected0RTT(endpointOptions, ticket, token) {
  const ep = await listen(mustCall(async (ss) => {
    await ss.closed;
  }), {
    sni,
    ...endpointOptions,
  });

  const cs = await connect(ep.address, {
    servername: 'localhost',
    ...endpointOptions,
    sessionTicket: ticket,
    token,
  });

  // Trigger the deferred handshake by opening a stream.
  // With 0-RTT, the handshake is deferred until the first stream
  // or datagram is sent. When 0-RTT is rejected, the stream is
  // destroyed by EarlyDataRejected — its closed promise rejects
  // with an application error.
  const s = await cs.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/rejected',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
  });
  await rejects(s.closed, {
    code: 'ERR_QUIC_APPLICATION_ERROR',
  });

  const info = await cs.opened;
  strictEqual(info.earlyDataAttempted, true);
  strictEqual(info.earlyDataAccepted, false);

  cs.close();
  ep.close();
}

const tokenSecret = randomBytes(16);

// enable_connect_protocol disabled.
{
  const { ticket, token } = await getTicket({
    endpoint: { tokenSecret },
    application: { enableConnectProtocol: true },
  });

  await attemptRejected0RTT({
    endpoint: { tokenSecret },
    // EnableConnectProtocol reduced from true to false.
    application: { enableConnectProtocol: false },
  }, ticket, token);
}

// enable_datagrams disabled.
{
  const { ticket, token } = await getTicket({
    endpoint: { tokenSecret },
    application: { enableDatagrams: true },
  });

  await attemptRejected0RTT({
    endpoint: { tokenSecret },
    // EnableDatagrams reduced from true to false.
    application: { enableDatagrams: false },
  }, ticket, token);
}

// max_field_section_size decreased.
{
  const { ticket, token } = await getTicket({
    endpoint: { tokenSecret },
    application: { maxFieldSectionSize: 10000 },
  });

  await attemptRejected0RTT({
    endpoint: { tokenSecret },
    // MaxFieldSectionSize reduced from 10000 to 100.
    application: { maxFieldSectionSize: 100 },
  }, ticket, token);
}
