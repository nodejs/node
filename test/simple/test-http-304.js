require('../common');

var sys = require('sys'),
  http = require('http'),
  childProcess = require('child_process');

s = http.createServer(function (request, response) {
  response.writeHead(304);
  response.end();
});
s.listen(PORT);
sys.puts('Server running at http://127.0.0.1:'+PORT+'/')

s.addListener('listening', function () {

  childProcess.exec('curl -i http://127.0.0.1:'+PORT+'/', function (err, stdout, stderr) {
    if (err) throw err;
    s.close();
    error('curled response correctly');
    error(sys.inspect(stdout));
  });

});

