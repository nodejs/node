'use strict';
const common = require('../common');
var http = require('http');

var server = http.createServer(common.mustCall(function(req, res) {
  res.end();
})).listen(0, common.mustCall(function() {
  var options = {
    agent: null,
    port: this.address().port
  };
  http.get(options, common.mustCall(function(res) {
    res.resume();
    server.close();
  }));
}));
