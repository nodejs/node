if (!process.versions.openssl) {
  console.error("Skipping because node compiled without OpenSSL.");
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');

var fs = require('fs');
var exec = require('child_process').exec;

var https = require('https');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var reqCount = 0;
var body = 'hello world\n';

var server = https.createServer(options, function (req, res) {
  reqCount++;
  console.log("got request");
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end(body);
})

function afterCurl (err, stdout, stderr) {
  if (err) throw err;
  server.close();
  common.error(common.inspect(stdout));
  assert.equal(body, stdout);
};

server.listen(common.PORT, function () {
  var cmd = 'curl --insecure https://127.0.0.1:' + common.PORT +  '/';
  console.error("executing %j", cmd);
  exec(cmd, afterCurl);
});

process.on('exit', function () {
  assert.equal(1, reqCount);
});
