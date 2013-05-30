var request = require('../main');
var http = require('http');
var requests = 0;
var assert = require('assert');

var server = http.createServer(function (req, res) {
  requests ++;

  // redirect everything 3 times, no matter what.
  var c = req.headers.cookie;

  if (!c) c = 0;
  else c = +c.split('=')[1] || 0;

  if (c > 3) {
    res.end('ok: '+requests);
    return;
  }

  res.setHeader('set-cookie', 'c=' + (c + 1));
  res.setHeader('location', req.url);
  res.statusCode = 302;
  res.end('try again, i guess\n');
});
server.listen(6767);

request.post({ url: 'http://localhost:6767/foo',
               followAllRedirects: true,
               form: { foo: 'bar' } }, function (er, req, body) {
  if (er) throw er;
  assert.equal(body, 'ok: 5');
  assert.equal(requests, 5);
  console.error('ok - ' + process.version);
  server.close();
});
