/*
test custom headers, added in pull request:
https://github.com/felixge/node-form-data/pull/17
*/

var common = require('../common');
var assert = common.assert;
var http = require('http');

var FormData = require(common.dir.lib + '/form_data');

var CRLF = '\r\n';

var testHeader = 'X-Test-Fake: 123';

var expectedLength;


var server = http.createServer(function(req, res) {
  var data = '';
  req.setEncoding('utf8');

  req.on('data', function(d) {
    data += d;
  });

  req.on('end', function() {
    assert.ok( data.indexOf( testHeader ) != -1 );

    // content-length would be 1000+ w/actual buffer size,
    // but smaller w/overridden size.
    assert.ok( typeof req.headers['content-length'] !== 'undefined' );
    assert.equal(req.headers['content-length'], expectedLength);

    res.writeHead(200);
    res.end('done');
  });
});


server.listen(common.port, function() {
  var form = new FormData();

  var options = {
    header:
      CRLF + '--' + form.getBoundary() + CRLF +
      testHeader +
      CRLF + CRLF,

    // override content-length,
    // much lower than actual buffer size (1000)
    knownLength: 1
  };

  var bufferData = [];
  for (var z = 0; z < 1000; z++) {
    bufferData.push(1);
  }
  var buffer = new Buffer(bufferData);

  form.append('my_buffer', buffer, options);

  // (available to req handler)
  expectedLength = form._lastBoundary().length + form._overheadLength + options.knownLength;

  form.submit('http://localhost:' + common.port + '/', function(err, res) {
    if (err) {
      throw err;
    }

    assert.strictEqual(res.statusCode, 200);
    server.close();
  });

});
