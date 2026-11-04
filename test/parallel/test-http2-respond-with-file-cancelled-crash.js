'use strict';

// Regression test for https://hackerone.com/4150894 (NULL dereference in
// StreamPipe). Cancelling an in-flight `respondWithFile()` download while a
// file read is still pending on the thread pool crashed the server: a NULL
// WriteWrap was forwarded to ReportWritesToJSStreamListener, which then
// dereferenced it in OnStreamAfterReqFinished().

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.on('error', () => {});
  // Use a file large enough that a read is still in flight when the client
  // destroys the session.
  stream.respondWithFile(process.execPath);
});
server.on('sessionError', () => {});

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const rounds = 50;
  let i = 0;
  let done = false;

  function cancelOne() {
    if (done) return;
    if (i >= rounds) {
      done = true;
      server.close(common.mustCall());
      return;
    }
    i++;
    const client = http2.connect(`http://127.0.0.1:${port}`);
    client.on('error', () => {});
    const req = client.request({ ':path': '/' });
    req.on('error', () => {});
    req.resume();
    // Destroy the session shortly after the request is sent, while the server
    // may still be reading the file. Vary the delay to hit the race window.
    setTimeout(() => {
      client.destroy();
      setTimeout(cancelOne, 1);
    }, 3 + (i * 7) % 40);
  }

  cancelOne();
}));
