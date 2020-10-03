// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

// The purpose of this test is to make sure that when forking a process,
// sending a fd representing a UDP socket to the child and sending messages
// to this endpoint, these messages are distributed to the parent and the
// child process.


const common = require('../common');
if (common.isWindows)
  common.skip('Sending dgram sockets to child processes is not supported');

const dgram = require('dgram');
const fork = require('child_process').fork;
const assert = require('assert');

if (process.argv[2] === 'child') {
  let childServer;

  process.once('message', (msg, clusterServer) => {
    childServer = clusterServer;

    childServer.once('message', () => {
      process.send('gotMessage');
      childServer.close();
    });

    process.send('handleReceived');
  });

} else {
  const parentServer = dgram.createSocket('udp4');
  const client = dgram.createSocket('udp4');
  const child = fork(__filename, ['child']);

  const msg = Buffer.from('Some bytes');

  let childGotMessage = false;
  let parentGotMessage = false;

  parentServer.once('message', (msg, rinfo) => {
    parentGotMessage = true;
    parentServer.close();
  });

  parentServer.on('listening', () => {
    child.send('server', parentServer);

    child.on('message', (msg) => {
      if (msg === 'gotMessage') {
        childGotMessage = true;
      } else if (msg === 'handleReceived') {
        sendMessages();
      }
    });
  });

  function sendMessages() {
    const serverPort = parentServer.address().port;

    const timer = setInterval(() => {
      // Both the parent and the child got at least one message,
      // test passed, clean up everything.
      if (parentGotMessage && childGotMessage) {
        clearInterval(timer);
        client.close();
      } else {
        client.send(
          msg,
          0,
          msg.length,
          serverPort,
          '127.0.0.1',
          (err) => {
            assert.ifError(err);
          }
        );
      }
    }, 1);
  }

  parentServer.bind(0, '127.0.0.1');

  process.once('exit', () => {
    assert(parentGotMessage);
    assert(childGotMessage);
  });
}
