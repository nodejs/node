process.mixin(require("./common"));

var PORT = 8889;
var http = require('http');
var sys = require('sys');
var modulesLoaded = 0;

var server = http.createServer(function(req, res) {
  var body = 'exports.httpPath = function() {'+
             'return '+JSON.stringify(req.uri.path)+';'+
             '};';

  res.sendHeader(200, {'Content-Type': 'text/javascript'});
  res.sendBody(body);
  res.finish();
});
server.listen(PORT);

var httpModule = require('http://localhost:'+PORT+'/moduleA.js');
assertEquals('/moduleA.js', httpModule.httpPath());
modulesLoaded++;

var nodeBinary = process.ARGV[0];
var cmd = nodeBinary+' http://localhost:'+PORT+'/moduleB.js';

sys
  .exec(cmd)
  .addCallback(function() {
    modulesLoaded++;
    server.close();
  })
  .addErrback(function() {
    assertUnreachable('node binary could not load module from url');
  });

process.addListener('exit', function() {
  assertEquals(2, modulesLoaded);
});