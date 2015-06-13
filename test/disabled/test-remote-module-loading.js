'use strict';
var common = require('../common');
var assert = require('assert');

var http = require('http');
var util = require('util');
var url = require('url');
var modulesLoaded = 0;

var server = http.createServer(function(req, res) {
  var body = 'exports.httpPath = function() {' +
             'return ' + JSON.stringify(url.parse(req.url).pathname) + ';' +
             '};';
  res.writeHead(200, {'Content-Type': 'text/javascript'});
  res.write(body);
  res.end();
});
server.listen(common.PORT);

assert.throws(function() {
  var httpModule = require('http://localhost:' + common.PORT + '/moduleA.js');
  assert.equal('/moduleA.js', httpModule.httpPath());
  modulesLoaded++;
});

var nodeBinary = process.ARGV[0];
var cmd = 'NODE_PATH=' + common.libDir + ' ' + nodeBinary +
          ' http://localhost:' + common.PORT + '/moduleB.js';

util.exec(cmd, function(err, stdout, stderr) {
  if (err) throw err;
  console.log('success!');
  modulesLoaded++;
  server.close();
});

process.on('exit', function() {
  assert.equal(1, modulesLoaded);
});
