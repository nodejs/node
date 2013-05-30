var request = require('../main');
var http = require('http');
var requests = 0;
var assert = require('assert');

var server = http.createServer(function (req, res) {
  console.error(req.method, req.url);
  requests ++;

  if (req.method === 'POST') {
    console.error('send 303');
    res.setHeader('location', req.url);
    res.statusCode = 303;
    res.end('try again, i guess\n');
  } else {
    console.error('send 200')
    res.end('ok: ' + requests);
  }
});
server.listen(6767);

request.post({ url: 'http://localhost:6767/foo',
               followAllRedirects: true,
               form: { foo: 'bar' } }, function (er, req, body) {
  if (er) throw er;
  assert.equal(body, 'ok: 2');
  assert.equal(requests, 2);
  console.error('ok - ' + process.version);
  server.close();
});
