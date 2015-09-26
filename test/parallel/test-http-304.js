'use strict';
var common = require('../common');
var assert = require('assert');

var http = require('http');
var childProcess = require('child_process');

var s = http.createServer(function(request, response) {
  response.writeHead(304);
  response.end();
});

s.listen(common.PORT, function() {
  childProcess.exec('curl -i http://127.0.0.1:' + common.PORT + '/',
                    function(err, stdout, stderr) {
                      if (err) throw err;
                      s.close();
                    });
});
