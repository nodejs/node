var common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.createServer(function(req, res) {
  res.end();
}).listen(common.PORT, function() {
  var opts = {
    host: 'localhost',
    port: common.PORT,
    headers: {
      'Host': 'localhost:' + common.PORT,
      'Connection': 'close'
    }
  };

  var agent = new http.Agent(opts);
  opts.agent = agent;
  opts.path = '/';
  var req = http.request(opts, function(res) {
    res.resume();
    server.close();
  });
  req.on('error', assert.fail);
  req.end();
});
