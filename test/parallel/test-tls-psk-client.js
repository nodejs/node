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
const IDENTITY = 'Client_identity';  // Hardcoded by openssl

const server = spawn(common.opensslCli, [
  's_server',
  '-accept', common.PORT,
  '-cipher', CIPHERS,
  '-psk', KEY,
  '-psk_hint', IDENTITY,
  '-nocert'
]);


let state = 'WAIT-ACCEPT';

let serverStdoutBuffer = '';
server.stdout.setEncoding('utf8');
server.stdout.on('data', (s) => {
  serverStdoutBuffer += s;
  switch (state) {
    case 'WAIT-ACCEPT':
      if (/ACCEPT/g.test(serverStdoutBuffer)) {
        // Give s_server half a second to start up.
        setTimeout(startClient, 500);
        state = 'WAIT-HELLO';
      }
      break;

    case 'WAIT-HELLO':
      if (/hello/g.test(serverStdoutBuffer)) {

        // End the current SSL connection and exit.
        // See s_server(1ssl).
        server.stdin.write('Q');

        state = 'WAIT-SERVER-CLOSE';
      }
      break;

    default:
      break;
  }
});


const timeout = setTimeout(() => {
  server.kill();
  process.exit(1);
}, 5000);

let gotWriteCallback = false;
let serverExitCode = -1;

server.on('exit', (code) => {
  serverExitCode = code;
  clearTimeout(timeout);
});


function startClient() {
  const s = tls.connect(common.PORT, {
    ciphers: CIPHERS,
    rejectUnauthorized: false,
    pskCallback: (hint) => {
      if (hint === IDENTITY) {
        return {
          identity: IDENTITY,
          psk: Buffer.from(KEY, 'hex')
        };
      }
    }
  });

  s.on('secureConnect', () => {
    s.write('hello\r\n', () => {
      gotWriteCallback = true;
    });
  });
}


process.on('exit', () => {
  assert.strictEqual(0, serverExitCode);
  assert.strictEqual('WAIT-SERVER-CLOSE', state);
  assert.ok(gotWriteCallback);
});
