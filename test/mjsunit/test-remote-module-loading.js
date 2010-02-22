process.mixin(require("./common"));

var PORT = 8889;
var http = require('http');
var sys = require('sys');
var url = require("url");
var modulesLoaded = 0;

var server = http.createServer(function(req, res) {
  var body = 'exports.httpPath = function() {'+
             'return '+JSON.stringify(url.parse(req.url).pathname)+';'+
             '};';

  res.writeHeader(200, {'Content-Type': 'text/javascript'});
  res.write(body);
  res.close();
});
server.listen(PORT);

assert.throws(function () {
  var httpModule = require('http://localhost:'+PORT+'/moduleA.js');
  assert.equal('/moduleA.js', httpModule.httpPath());
  modulesLoaded++;
});

var nodeBinary = process.ARGV[0];
var cmd = 'NODE_PATH='+libDir+' '+nodeBinary+' http://localhost:'+PORT+'/moduleB.js';

sys.exec(cmd, function (err, stdout, stderr) {
  if (err) throw err;
  puts('success!');
  modulesLoaded++;
  server.close();
});

process.addListener('exit', function() {
  assert.equal(1, modulesLoaded);
});
