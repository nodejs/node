'use strict';

/* eslint-disable no-void */

// PGO Training Script: Network (TCP) and DNS
//
// Exercises the core networking primitives used by every HTTP server/client:
// - TCP server/client (the foundation of HTTP)
// - DNS resolution (every outbound connection)
// - Connection pooling patterns (keep-alive, max sockets)
// - Various data transfer patterns (small messages, large payloads, streaming)
// - Unix domain sockets / named pipes on Windows (IPC)
//
// This exercises: libuv TCP/pipe handles, DNS resolver (c-ares),
// socket state machine, Buffer transfers, EventEmitter for net events.

const net = require('net');
const dns = require('dns');
const os = require('os');
const path = require('path');
const crypto = require('crypto');

const DURATION_MS = parseInt(process.env.PGO_TRAINING_DURATION, 10) || 15_000;
const PORT_TCP = 0; // OS-assigned

// Test payloads simulating real protocols
const SMALL_MSG = Buffer.from('{"type":"ping","ts":1234567890}\n');
const MEDIUM_MSG = Buffer.from(
  JSON.stringify({
    type: 'data',
    payload: Array.from({ length: 100 }, (_, i) => ({
      key: `item_${i}`,
      value: crypto.randomBytes(16).toString('hex'),
    })),
  }) + '\n',
);
const LARGE_MSG = crypto.randomBytes(64 * 1024);

// Workload 1: TCP echo server with concurrent clients
async function workloadTCPEcho(duration) {
  let totalOps = 0;

  const server = net.createServer((socket) => {
    socket.on('data', (data) => {
      // Echo back with a small transformation (realistic: add header/length prefix)
      const response = Buffer.concat([Buffer.from(`${data.length}:`), data]);
      socket.write(response);
    });
    socket.on('error', () => {});
  });

  await new Promise((resolve) => server.listen(PORT_TCP, '127.0.0.1', resolve));
  const port = server.address().port;

  const endTime = Date.now() + duration;

  async function runClient() {
    let ops = 0;
    while (Date.now() < endTime) {
      await new Promise((resolve) => {
        const client = net.createConnection({ port, host: '127.0.0.1' }, () => {
          let msgsSent = 0;
          const maxMsgs = 50;

          function sendNext() {
            if (msgsSent >= maxMsgs || Date.now() >= endTime) {
              client.end();
              return;
            }
            // Vary message sizes (realistic: mostly small, some medium, rare large)
            const r = Math.random();
            const msg = r < 0.7 ? SMALL_MSG : r < 0.95 ? MEDIUM_MSG : LARGE_MSG;
            client.write(msg);
            msgsSent++;
            ops++;
          }

          client.on('data', () => {
            sendNext();
          });
          sendNext();
        });

        client.on('end', resolve);
        client.on('error', () => resolve());
        client.setTimeout(2000, () => {
          client.destroy();
          resolve();
        });
      });
    }
    return ops;
  }

  const clients = Array.from({ length: 10 }, () => runClient());
  const results = await Promise.all(clients);
  totalOps = results.reduce((a, b) => a + b, 0);

  server.close();
  return totalOps;
}

// Workload 2: TCP request/response (HTTP-like pattern without HTTP overhead)
async function workloadTCPRequestResponse(duration) {
  let totalOps = 0;

  const server = net.createServer((socket) => {
    let buffer = '';
    socket.on('data', (data) => {
      buffer += data.toString();
      // Simple line-delimited protocol (like Redis, memcached)
      const lines = buffer.split('\n');
      buffer = lines.pop() || '';

      for (const line of lines) {
        if (!line) continue;
        try {
          const req = JSON.parse(line);
          let response;

          switch (req.cmd) {
            case 'GET':
              response = {
                status: 'ok',
                key: req.key,
                value: 'cached_value_' + req.key,
              };
              break;
            case 'SET':
              response = { status: 'ok', key: req.key };
              break;
            case 'DEL':
              response = { status: 'ok', deleted: 1 };
              break;
            case 'MGET':
              response = {
                status: 'ok',
                values: req.keys.map((k) => 'val_' + k),
              };
              break;
            default:
              response = { status: 'error', message: 'Unknown command' };
          }
          socket.write(JSON.stringify(response) + '\n');
        } catch {
          socket.write('{"status":"error","message":"Parse error"}\n');
        }
      }
    });
    socket.on('error', () => {});
  });

  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const port = server.address().port;

  const endTime = Date.now() + duration;

  async function runClient() {
    let ops = 0;
    while (Date.now() < endTime) {
      await new Promise((resolve) => {
        const client = net.createConnection({ port, host: '127.0.0.1' }, () => {
          let pending = 0;
          const maxOps = 100;

          function sendNext() {
            if (pending >= maxOps || Date.now() >= endTime) {
              client.end();
              return;
            }

            const commands = [
              { cmd: 'GET', key: `user:${pending}` },
              {
                cmd: 'SET',
                key: `session:${pending}`,
                value: crypto.randomUUID(),
              },
              { cmd: 'MGET', keys: ['key1', 'key2', 'key3'] },
              { cmd: 'DEL', key: `temp:${pending}` },
            ];
            const cmd = commands[pending % commands.length];
            client.write(JSON.stringify(cmd) + '\n');
            pending++;
            ops++;
          }

          client.on('data', () => {
            sendNext();
          });
          sendNext();
        });

        client.on('end', resolve);
        client.on('error', () => resolve());
        client.setTimeout(2000, () => {
          client.destroy();
          resolve();
        });
      });
    }
    return ops;
  }

  const clients = Array.from({ length: 8 }, () => runClient());
  const results = await Promise.all(clients);
  totalOps = results.reduce((a, b) => a + b, 0);

  server.close();
  return totalOps;
}

// Workload 3: Named pipe / IPC (very common on Windows - VS Code, build tools)
async function workloadIPC(duration) {
  let totalOps = 0;
  const pipePath =
    process.platform === 'win32' ?
      `\\\\.\\pipe\\node-pgo-${process.pid}-${Date.now()}` :
      path.join(os.tmpdir(), `node-pgo-ipc-${process.pid}.sock`);

  const server = net.createServer((socket) => {
    socket.on('data', (data) => {
      // IPC protocol: length-prefixed messages
      const msg = JSON.parse(data.toString());
      const response = {
        id: msg.id,
        result: msg.method === 'eval' ? 'ok' : 'unknown',
        data: { processed: true, timestamp: Date.now() },
      };
      const buf = Buffer.from(JSON.stringify(response));
      socket.write(buf);
    });
    socket.on('error', () => {});
  });

  await new Promise((resolve) => server.listen(pipePath, resolve));

  const endTime = Date.now() + duration;

  async function runClient() {
    let ops = 0;
    while (Date.now() < endTime) {
      await new Promise((resolve) => {
        const client = net.createConnection(pipePath, () => {
          let count = 0;
          const maxMsgs = 50;

          function sendNext() {
            if (count >= maxMsgs || Date.now() >= endTime) {
              client.end();
              return;
            }
            const msg = {
              id: count,
              method: ['eval', 'complete', 'lint', 'format'][count % 4],
              params: { file: `src/file${count}.ts`, line: count * 2 },
            };
            client.write(JSON.stringify(msg));
            count++;
            ops++;
          }

          client.on('data', () => sendNext());
          sendNext();
        });

        client.on('end', resolve);
        client.on('error', () => resolve());
        client.setTimeout(2000, () => {
          client.destroy();
          resolve();
        });
      });
    }
    return ops;
  }

  const clients = Array.from({ length: 4 }, () => runClient());
  const results = await Promise.all(clients);
  totalOps = results.reduce((a, b) => a + b, 0);

  server.close();
  // Cleanup socket file on Unix
  if (process.platform !== 'win32') {
    try {
      require('fs').unlinkSync(pipePath);
    } catch {
      // best effort
    }
  }
  return totalOps;
}

// Workload 4: DNS resolution (every HTTP client connection does this)
async function workloadDNS(iterations) {
  let ops = 0;

  // Use localhost and standard DNS patterns
  const hosts = ['localhost', '127.0.0.1', '::1'];

  for (let i = 0; i < iterations; i++) {
    // dns.lookup (uses OS resolver — libuv thread pool)
    for (const host of hosts) {
      try {
        await new Promise((resolve, reject) => {
          dns.lookup(host, (err, address) => {
            if (err) reject(err);
            else resolve(address);
          });
        });
        ops++;
      } catch {
        ops++;
      }
    }

    // dns.lookup with options (IPv4/IPv6 preference)
    try {
      await new Promise((resolve, reject) => {
        dns.lookup('localhost', { family: 4 }, (err, address) => {
          if (err) reject(err);
          else resolve(address);
        });
      });
      ops++;
    } catch {
      ops++;
    }

    // dns.lookupService (reverse lookup)
    try {
      await new Promise((resolve, reject) => {
        dns.lookupService('127.0.0.1', 80, (err, hostname, service) => {
          if (err) reject(err);
          else resolve({ hostname, service });
        });
      });
      ops++;
    } catch {
      ops++;
    }

    // net.isIP / net.isIPv4 / net.isIPv6 (validation in every request)
    net.isIP('192.168.1.1');
    net.isIP('::1');
    net.isIP('not-an-ip');
    net.isIPv4('192.168.1.1');
    net.isIPv6('::1');
    net.isIPv6('fe80::1%eth0');
    ops += 6;
  }
  return ops;
}

// Workload 5: Socket options and state transitions
async function workloadSocketOps(duration) {
  let totalOps = 0;

  const server = net.createServer((socket) => {
    // Exercise socket properties (common in logging, monitoring)
    void socket.remoteAddress;
    void socket.remotePort;
    void socket.localAddress;
    void socket.localPort;
    void socket.bytesRead;
    void socket.bytesWritten;
    void socket.readyState;

    socket.setNoDelay(true);
    socket.setKeepAlive(true, 1000);

    socket.on('data', (data) => {
      socket.write(data);
    });
    socket.on('error', () => {});
  });

  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const port = server.address().port;

  const endTime = Date.now() + duration;

  // Rapid connect/disconnect (connection churn — load balancer pattern)
  while (Date.now() < endTime) {
    await new Promise((resolve) => {
      const client = net.createConnection({ port, host: '127.0.0.1' }, () => {
        client.setNoDelay(true);
        client.write('ping');
      });
      client.on('data', () => {
        totalOps++;
        client.end();
      });
      client.on('end', resolve);
      client.on('error', () => resolve());
      client.setTimeout(1000, () => {
        client.destroy();
        resolve();
      });
    });
  }

  server.close();
  return totalOps;
}

async function main() {
  console.log('[pgo-net] Starting network workload...');
  const startTime = Date.now();
  let totalOps = 0;

  const timeBudget = (fraction) => Math.floor(DURATION_MS * fraction);

  // TCP echo (data transfer throughput)
  console.log('[pgo-net] Running TCP echo...');
  totalOps += await workloadTCPEcho(timeBudget(0.25));

  // TCP request/response (protocol handling)
  console.log('[pgo-net] Running TCP request/response...');
  totalOps += await workloadTCPRequestResponse(timeBudget(0.25));

  // IPC / Named pipes
  console.log('[pgo-net] Running IPC...');
  totalOps += await workloadIPC(timeBudget(0.15));

  // DNS resolution
  console.log('[pgo-net] Running DNS...');
  totalOps += await workloadDNS(100);

  // Socket operations
  console.log('[pgo-net] Running socket ops...');
  totalOps += await workloadSocketOps(timeBudget(0.15));

  const elapsed = (Date.now() - startTime) / 1000;
  console.log(
    `[pgo-net] Completed ${totalOps} net operations in ${elapsed.toFixed(1)}s (${(totalOps / elapsed).toFixed(0)} ops/s)`,
  );
}

main().catch((err) => {
  console.error('[pgo-net] Error:', err);
  process.exit(1);
});
