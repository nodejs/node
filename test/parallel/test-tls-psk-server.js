'use strict';
const common = require('../common');

if (!common.opensslCli) {
  console.log('1..0 # Skipped: node compiled without OpenSSL CLI.');
  return;
}

const assert = require('assert');

const tls = require('tls');
const spawn = require('child_process').spawn;

const CIPHERS = 'PSK+HIGH';
const KEY = 'd731ef57be09e5204f0b205b60627028';
const IDENTITY = 'TestUser';

let connections = 0;

const server = tls.createServer({
  ciphers: CIPHERS,
  pskIdentity: IDENTITY,
  pskCallback: (identity) => {
    if (identity === IDENTITY) {
      return {
        psk: Buffer.from(KEY, 'hex')
      };
    }
  }
});

server.on('connection', (socket) => {
  connections++;
});

server.on('secureConnection', (socket) => {
  socket.write('hello\r\n');

  socket.on('data', (data) => {
    socket.write(data);
  });
});

let gotHello = false;
let sentWorld = false;
let gotWorld = false;
let opensslExitCode = -1;

server.listen(common.PORT, () => {
  const client = spawn(common.opensslCli, [
    's_client',
    '-connect', '127.0.0.1:' + common.PORT,
    '-cipher', CIPHERS,
    '-psk', KEY,
    '-psk_identity', IDENTITY
  ]);

  let out = '';

  client.stdout.setEncoding('utf8');
  client.stdout.on('data', (d) => {
    out += d;

    if (!gotHello && /hello/.test(out)) {
      gotHello = true;
      client.stdin.write('world\r\n');
      sentWorld = true;
    }

    if (!gotWorld && /world/.test(out)) {
      gotWorld = true;
      client.stdin.end();
    }
  });

  client.on('exit', (code) => {
    opensslExitCode = code;
    server.close();
  });
});

process.on('exit', () => {
  assert.strictEqual(1, connections);
  assert.ok(gotHello);
  assert.ok(sentWorld);
  assert.ok(gotWorld);
  assert.strictEqual(0, opensslExitCode);
});
