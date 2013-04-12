var http = require('http')
  , assert = require('assert')
  , request = require('../index')
  ;

var portOne = 8968
  , portTwo = 8969
  ;


// server one
var s1 = http.createServer(function (req, resp) {
  if (req.url == '/original') {
    resp.writeHeader(302, {'location': '/redirected'})
    resp.end()
  } else if (req.url == '/redirected') {
    resp.writeHeader(200, {'content-type': 'text/plain'})
    resp.write('OK')
    resp.end()
  }

}).listen(portOne);


// server two
var s2 = http.createServer(function (req, resp) {
  var x = request('http://localhost:'+portOne+'/original')
  req.pipe(x)
  x.pipe(resp)

}).listen(portTwo, function () {
  var r = request('http://localhost:'+portTwo+'/original', function (err, res, body) {
    assert.equal(body, 'OK')

    s1.close()
    s2.close()
  });

  // it hangs, so wait a second :)
  r.timeout = 1000;

})
