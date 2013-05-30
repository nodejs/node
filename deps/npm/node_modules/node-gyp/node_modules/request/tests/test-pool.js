var request = require('../main')
  , http = require('http')
  , assert = require('assert')
  ;

var s = http.createServer(function (req, resp) {
  resp.statusCode = 200;
  resp.end('asdf');
}).listen(8080, function () {
  request({'url': 'http://localhost:8080', 'pool': false}, function (e, resp) {
    var agent = resp.request.agent;
    assert.strictEqual(typeof agent, 'boolean');
    assert.strictEqual(agent, false);
    s.close();
  });
});