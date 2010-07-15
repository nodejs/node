common = require("../common");
assert = common.assert

http = require('http');
childProcess = require('child_process');

s = http.createServer(function (request, response) {
  response.writeHead(304);
  response.end();
});

s.listen(common.PORT, function () {
  childProcess.exec('curl -i http://127.0.0.1:'+common.PORT+'/', function (err, stdout, stderr) {
    if (err) throw err;
    s.close();
    common.error('curled response correctly');
    common.error(common.inspect(stdout));
  });
});

console.log('Server running at http://127.0.0.1:'+common.PORT+'/')
