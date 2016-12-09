'use strict';
const common = require('../common');
var http = require('http');
var url = require('url');

var server = http.createServer(common.mustCall(function(req, res) {
  res.end();
})).listen(0, '127.0.0.1', common.mustCall(function() {
  var opts = url.parse(`http://127.0.0.1:${this.address().port}/`);

  // remove the `protocol` field… the `http` module should fall back
  // to "http:", as defined by the global, default `http.Agent` instance.
  opts.agent = new http.Agent();
  opts.agent.protocol = null;

  http.get(opts, common.mustCall(function(res) {
    res.resume();
    server.close();
  }));
}));
