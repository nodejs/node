var common = require('../common');
var http = require('http');
var net = require('net');
var url = require('url');
var assert = require('assert');

// Response splitting example, credit: Amit Klein, Safebreach
var str = '/welcome?lang=bar%c4%8d%c4%8aContent­Length:%200%c4%8d%c4%8a%c' +
          '4%8d%c4%8aHTTP/1.1%20200%20OK%c4%8d%c4%8aContent­Length:%202' +
          '0%c4%8d%c4%8aLast­Modified:%20Mon,%2027%20Oct%202003%2014:50:18' +
          '%20GMT%c4%8d%c4%8aContent­Type:%20text/html%c4%8d%c4%8a%c4%8' +
          'd%c4%8a%3chtml%3eGotcha!%3c/html%3e';

// Response splitting example, credit: Сковорода Никита Андреевич (@ChALkeR)
var x = 'fooഊSet-Cookie: foo=barഊഊ<script>alert("Hi!")</script>';
var y = 'foo⠊Set-Cookie: foo=bar';

var count = 0;

var server = http.createServer(function(req, res) {
  switch (count++) {
    case 0:
      var loc = url.parse(req.url, true).query.lang;
      assert.throws(common.mustCall(function() {
        res.writeHead(302, {Location: '/foo?lang=' + loc});
      }));
      break;
    case 1:
      assert.throws(common.mustCall(function() {
        res.writeHead(200, {'foo' : x});
      }));
      break;
    case 2:
      assert.throws(common.mustCall(function() {
        res.writeHead(200, {'foo' : y});
      }));
      break;
    default:
      assert.fail(null, null, 'should not get to here.');
  }
  if (count === 3)
    server.close();
  res.end('ok');
});
server.listen(common.PORT, function() {
  var end = 'HTTP/1.1\r\n\r\n';
  var client = net.connect({port: common.PORT}, function() {
    client.write('GET ' + str + ' ' + end);
    client.write('GET / ' + end);
    client.write('GET / ' + end);
    client.end();
  });
});