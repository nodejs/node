require('../common');

var sys = require('sys'),
  http = require('http'),
  childProcess = require('child_process');

s = http.createServer(function (request, response) {
  response.writeHead(304);
  response.end();
})
s.listen(8000);

childProcess.exec('curl http://127.0.0.1:8000/', function (err, stdout, stderr) {
  if (err) throw err;
  s.close();
  sys.puts('curled response correctly');
});

sys.puts('Server running at http://127.0.0.1:8000/')
