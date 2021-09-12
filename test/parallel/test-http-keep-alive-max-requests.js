'use strict';

const net = require('net');
const http = require('http');
const assert = require('assert');
const common = require('../common');

const bodySent = 'This is my request';

function assertResponse(headers, body, expectClosed) {
  if (expectClosed) {
    assert.match(headers, /Connection: close\r\n/m);
    assert(headers.search(/Keep-Alive: timeout=5, max=3\r\n/m) === -1);
    assert.match(body, /Hello World!/m);
  } else {
    assert.match(headers, /Connection: keep-alive\r\n/m);
    assert.match(headers, /Keep-Alive: timeout=5, max=3\r\n/m);
    assert.match(body, /Hello World!/m);
  }
}

function writeRequest(socket, withBody) {
  if (withBody) {
    socket.write('POST / HTTP/1.1\r\n');
    socket.write('Connection: keep-alive\r\n');
    socket.write('Content-Type: text/plain\r\n');
    socket.write(`Content-Length: ${bodySent.length}\r\n\r\n`);
    socket.write(`${bodySent}\r\n`);
    socket.write('\r\n\r\n')
  } else {
    socket.write('GET / HTTP/1.1\r\n');
    socket.write('Connection: keep-alive\r\n');
    socket.write('\r\n\r\n')
  }
}

const server = http.createServer(function (req, res) {
  let body = ''
  req.on('data', (data) => {
    body += data
  });

  req.on('end', () => {
    if (req.method === 'POST') {
      assert(bodySent === body)
    }
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write('Hello World!');
    res.end();
  })
})

server.maxRequestsPerSocket = 3;
server.listen(0, common.mustCall((res) => {
  const socket = net.createConnection(
    { port: server.address().port },
    common.mustCall(() => {
      writeRequest(socket)
      writeRequest(socket)

      const anotherSocket = net.createConnection(
        { port: server.address().port },
        common.mustCall(() => {
          writeRequest(anotherSocket)

          let anotherBuffer = ''
          let lastWritten = false;
          anotherSocket.setEncoding('utf8');
          anotherSocket.on('data', (data) => {
            anotherBuffer += data;

            if (anotherBuffer.endsWith('\r\n\r\n')) {
              if (lastWritten) {
                anotherSocket.end()
              } else {
                writeRequest(anotherSocket);
                lastWritten = true;
              }
            }
          });

          anotherSocket.on('end', common.mustCall(() => {
            const anoterResponses = anotherBuffer.trim().split('\r\n\r\n');

            assertResponse(anoterResponses[0], anoterResponses[1], false)
            assertResponse(anoterResponses[2], anoterResponses[3], false)

            // Add two additional requests to two previous on the first socket
            writeRequest(socket, true)
            writeRequest(socket, true)

            let buffer = '';
            socket.setEncoding('utf8');
            socket.on('data', (data) => {
                buffer += data;
            });

            socket.on('end', common.mustCall(() => {
              const responses = buffer.trim().split('\r\n\r\n');
              // We sent more requests than allowed per socket, 
              // but we get only the allowed number of responses & headers
              assert(responses.length === server.maxRequestsPerSocket * 2);

              assertResponse(responses[0], responses[1], false)
              assertResponse(responses[2], responses[3], false)
              assertResponse(responses[4], responses[5], true)

              server.close();
            }));
          }));
        }));
    })
  );
}));