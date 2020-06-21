// Flags: --expose-internals
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { createQuicSocket } = require('net');

const { recordState } = require('../common/heap');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

{
  const state = recordState();
  state.validateSnapshotNodes('Node / QuicStream', []);
  state.validateSnapshotNodes('Node / QuicSession', []);
  state.validateSnapshotNodes('Node / QuicSocket', []);
}

const server = createQuicSocket({ port: 0, validateAddress: true });

server.listen({
  key,
  cert,
  ca,
  rejectUnauthorized: false,
  maxCryptoBuffer: 4096,
  alpn: 'meow'
});

server.on('session', common.mustCall((session) => {
  session.on('secure', common.mustCall((servername, alpn, cipher) => {
    // eslint-disable-next-line no-unused-vars
    const stream = session.openStream({ halfOpen: false });

    const state = recordState();

    state.validateSnapshotNodes('Node / QuicSocket', [
      {
        children: [
          { node_name: 'QuicSocket', edge_name: 'wrapped' },
          { node_name: 'BigUint64Array', edge_name: 'stats_buffer' },
          { node_name: 'Node / sessions', edge_name: 'sessions' },
          { node_name: 'Node / dcid_to_scid', edge_name: 'dcid_to_scid' },
        ]
      }
    ], { loose: true });

    state.validateSnapshotNodes('Node / QuicStream', [
      {
        children: [
          { node_name: 'QuicStream', edge_name: 'wrapped' },
          { node_name: 'BigUint64Array', edge_name: 'stats_buffer' },
          { node_name: 'Node / QuicBuffer', edge_name: 'buffer' },
          { node_name: 'Node / HistogramBase', edge_name: 'data_rx_rate' },
          { node_name: 'Node / HistogramBase', edge_name: 'data_rx_size' },
          { node_name: 'Node / HistogramBase', edge_name: 'data_rx_ack' }
        ]
      }
    ], { loose: true });

    state.validateSnapshotNodes('Node / QuicBuffer', [
      {
        children: [
          { node_name: 'Node / length', edge_name: 'length' }
        ]
      }
    ], { loose: true });

    state.validateSnapshotNodes('Node / QuicSession', [
      {
        children: [
          { node_name: 'QuicServerSession', edge_name: 'wrapped' },
          { node_name: 'Node / QuicCryptoContext',
            edge_name: 'crypto_context' },
          { node_name: 'Node / HistogramBase', edge_name: 'crypto_rx_ack' },
          { node_name: 'Node / HistogramBase',
            edge_name: 'crypto_handshake_rate' },
          { node_name: 'Node / Timer', edge_name: 'retransmit' },
          { node_name: 'Node / Timer', edge_name: 'idle' },
          { node_name: 'Node / QuicBuffer', edge_name: 'sendbuf' },
          { node_name: 'Node / QuicBuffer', edge_name: 'txbuf' },
          { node_name: 'Float64Array', edge_name: 'recovery_stats_buffer' },
          { node_name: 'BigUint64Array', edge_name: 'stats_buffer' },
          { node_name: 'Node / current_ngtcp2_memory',
            edge_name: 'current_ngtcp2_memory' },
          { node_name: 'Node / streams', edge_name: 'streams' },
          { node_name: 'Node / std::basic_string', edge_name: 'alpn' },
          { node_name: 'Node / std::basic_string', edge_name: 'hostname' },
          { node_name: 'Float64Array', edge_name: 'state' },
        ]
      },
      {
        children: [
          { node_name: 'QuicClientSession', edge_name: 'wrapped' },
          { node_name: 'Node / QuicCryptoContext',
            edge_name: 'crypto_context' },
          { node_name: 'Node / HistogramBase', edge_name: 'crypto_rx_ack' },
          { node_name: 'Node / HistogramBase',
            edge_name: 'crypto_handshake_rate' },
          { node_name: 'Node / Timer', edge_name: 'retransmit' },
          { node_name: 'Node / Timer', edge_name: 'idle' },
          { node_name: 'Node / QuicBuffer', edge_name: 'sendbuf' },
          { node_name: 'Node / QuicBuffer', edge_name: 'txbuf' },
          { node_name: 'Float64Array', edge_name: 'recovery_stats_buffer' },
          { node_name: 'BigUint64Array', edge_name: 'stats_buffer' },
          { node_name: 'Node / current_ngtcp2_memory',
            edge_name: 'current_ngtcp2_memory' },
          { node_name: 'Node / streams', edge_name: 'streams' },
          { node_name: 'Node / std::basic_string', edge_name: 'alpn' },
          { node_name: 'Node / std::basic_string', edge_name: 'hostname' },
          { node_name: 'Float64Array', edge_name: 'state' },
        ]
      }
    ], { loose: true });

    state.validateSnapshotNodes('Node / QuicCryptoContext', [
      {
        children: [
          { node_name: 'Node / rx_secret', edge_name: 'rx_secret' },
          { node_name: 'Node / tx_secret', edge_name: 'tx_secret' },
          { node_name: 'Node / QuicBuffer', edge_name: 'initial_crypto' },
          { node_name: 'Node / QuicBuffer',
            edge_name: 'handshake_crypto' },
          { node_name: 'Node / QuicBuffer', edge_name: 'app_crypto' },
        ]
      }
    ], { loose: true });

    session.destroy();
    server.close();
  }));
}));

server.on('ready', common.mustCall(() => {
  const client = createQuicSocket({
    port: 0,
    client: {
      key,
      cert,
      ca,
      alpn: 'meow'
    }
  });

  client.connect({
    address: 'localhost',
    port: server.address.port
  }).on('close', common.mustCall(() => client.close()));
}));
