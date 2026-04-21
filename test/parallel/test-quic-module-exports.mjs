// Flags: --experimental-quic --no-warnings

// Test: module exports completeness (CONST-06, CONST-07, CONST-08,
// CONST-09).
// Module exports are sealed.
// Stats classes exist on constructors.
// session ticket getter works after handshake.
// session token getter works after NEW_TOKEN.

import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, throws } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const quic = await import('node:quic');
const { listen, connect } = await import('../common/quic.mjs');

// Module exports are frozen/sealed.
throws(() => { quic.newProperty = true; }, TypeError);

// Stats classes exist.
ok(quic.QuicEndpoint);

// CONST-08/09: Session ticket and token getters.
{
  let savedTicket;
  let savedToken;
  const gotTicket = Promise.withResolvers();
  const gotToken = Promise.withResolvers();

  const serverEndpoint = await listen(async (serverSession) => {
    await serverSession.closed;
  });

  const clientSession = await connect(serverEndpoint.address, {
    onsessionticket(ticket) {
      savedTicket = ticket;
      gotTicket.resolve();
    },
    onnewtoken(token) {
      savedToken = token;
      gotToken.resolve();
    },
  });
  await Promise.all([clientSession.opened, gotTicket.promise, gotToken.promise]);

  // Session ticket is a Buffer.
  ok(Buffer.isBuffer(savedTicket));
  ok(savedTicket.length > 0);

  // Token is a Buffer.
  ok(Buffer.isBuffer(savedToken));
  ok(savedToken.length > 0);

  await clientSession.close();
  await serverEndpoint.close();
}
