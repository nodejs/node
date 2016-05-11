'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');
const net = require('net');

var passed = false;

const server = http.createServer((req, res) => {
  // The request send by the client includes a blank line between the
  // start line and the first header field. RFC 7230 requires that this
  // either be ignored or the message be rejected. Node.js, however,
  // ignores the headers completely and dispatches the request as if
  // no headers were present at all. That is, Node.js currently interprets
  // the headers as being part of the body payload of the message.

  // A successfull test would either mean that this callback is not called
  // at all and a clientError is emitted or that req.headers.host is not
  // undefined.
  assert(req.headers.host, 'req.headers.host is undefined.');
  res.end('ok');
  passed = true;
}).on('clientError', () => {
  // If we get a clientError, that means Node.js rejected the request, which
  // is a valid response to having a blank line between the first line and
  // the headers. Node.js could also choose to simply ignore the blank line.
  passed = true;
});

server.listen(common.PORT, common.mustCall(() => {
  // Per: RFC7230 -
  //  A sender MUST NOT send whitespace between the start-line and the
  //  first header field.  A recipient that receives whitespace between the
  //  start-line and the first header field MUST either reject the message
  //  as invalid or consume each whitespace-preceded line without further
  //  processing of it (i.e., ignore the entire line, along with any
  //  subsequent lines preceded by whitespace, until a properly formed
  //  header field is received or the header section is terminated).
  const client = net.connect({port: common.PORT}, common.mustCall(() => {
    // The following sends a request like:
    //
    //   GET / HTTP/1.1
    //
    //   Host: foo
    //
    // .. that is, it drops a blank line between the start-line and
    // the start of the first header field. Per the requirements in
    // RFC7230, this blank either needs to be rejected or ignored.
    client.write('GET / HTTP/1.1\r\n\r\nHost: foo\r\n\r\n');
    client.on('data', () => {
      server.close();
      client.end();
    });
    client.on('error', () => {
      server.close();
      client.end();
    });
  }));
}));

process.on('exit', () => {
  assert(passed);
});
