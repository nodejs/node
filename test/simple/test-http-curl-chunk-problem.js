// http://groups.google.com/group/nodejs/browse_thread/thread/f66cd3c960406919
var common = require('../common');
var assert = require('assert');
var http = require('http'),
    cp = require('child_process');


var filename = require('path').join(common.tmpDir || '/tmp', 'big');

var count = 0;
function maybeMakeRequest() {
  if (++count < 2) return;
  console.log('making curl request');
  cp.exec('curl http://127.0.0.1:' + common.PORT + '/ | shasum',
          function(err, stdout, stderr) {
            if (err) throw err;
            assert.equal('8c206a1a87599f532ce68675536f0b1546900d7a',
                         stdout.slice(0, 40));
            console.log('got the correct response');
            server.close();
          });
}


cp.exec('dd if=/dev/zero of=' + filename + ' bs=1024 count=10240',
        function(err, stdout, stderr) {
          if (err) throw err;
          maybeMakeRequest();
        });


var server = http.createServer(function(req, res) {
  res.writeHead(200);

  // Create the subprocess
  var cat = cp.spawn('cat', [filename]);

  // Stream the data through to the response as binary chunks
  cat.stdout.on('data', function(data) {
    res.write(data);
  });

  // End the response on exit (and log errors)
  cat.on('exit', function(code) {
    if (code !== 0) {
      console.error('subprocess exited with code ' + code);
      exit(1);
    }
    res.end();
  });

});

server.listen(common.PORT, maybeMakeRequest);

console.log('Server running at http://localhost:8080');
