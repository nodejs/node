var common = require('../common');
var http = require('http');
var assert = require('assert');

var MAX_COUNT = 2;

var server = http.createServer(function(req, res) {
  var num = req.headers['x-num'];
  // TODO(@jasnell) At some point this should be refactored as the API
  // should not be allowing users to set multiple content-length values
  // in the first place.
  switch (num) {
    case '1':
      res.setHeader('content-length', [2, 1]);
      break;
    case '2':
      res.writeHead(200, {'content-length': [1, 2]});
      break;
    default:
      assert.fail(null, null, 'should never get here');
  }
  res.end('ok');
});

var count = 0;

server.listen(common.PORT, common.mustCall(function() {
  for (var n = 1; n <= MAX_COUNT ; n++) {
    // This runs twice, the first time, the server will use
    // setHeader, the second time it uses writeHead. In either
    // case, the error handler must be called because the client
    // is not allowed to accept multiple content-length headers.
    http.get(
      {port:common.PORT, headers:{'x-num': n}},
      function(res) {
        assert(false, 'client allowed multiple content-length headers.');
      }
    ).on('error', common.mustCall(function(err) {
      assert(/^Parse Error/.test(err.message));
      assert.equal(err.code, 'HPE_UNEXPECTED_CONTENT_LENGTH');
      count++;
      if (count === MAX_COUNT)
        server.close();
    }));
  }
}));

process.on('exit', function() {
  assert.equal(count, MAX_COUNT);
});