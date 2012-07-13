
var assert = require('assert'),
    fs = require('fs'),
    path = require('path'),
    tls = require('tls');


var target_connections = 10000,
    concurrency = 10;

for (var i = 2; i < process.argv.length; i++) {
  switch (process.argv[i]) {
    case '-c':
      concurrency = ~~process.argv[++i];
      break;

    case '-n':
      target_connections = ~~process.argv[++i];
      break;

    default:
      throw new Error('Invalid flag: ' + process.argv[i]);
  }
}


var cert_dir = path.resolve(__dirname, '../test/fixtures'),
    options = { key: fs.readFileSync(cert_dir + '/test_key.pem'),
                cert: fs.readFileSync(cert_dir + '/test_cert.pem'),
                ca: [ fs.readFileSync(cert_dir + '/test_ca.pem') ] };

var server = tls.createServer(options, onConnection);
server.listen(8000);


var initiated_connections = 0,
    server_connections = 0,
    client_connections = 0,
    start = Date.now();

for (var i = 0; i < concurrency; i++)
  makeConnection();


process.on('exit', onExit);


function makeConnection() {
  if (initiated_connections >= target_connections)
    return;

  initiated_connections++;

  var conn = tls.connect(8000, function() {
    client_connections++;

    if (client_connections % 100 === 0)
      console.log(client_connections + ' of ' + target_connections +
                  ' connections made');

    conn.end();
    makeConnection();
  });
}


function onConnection(conn) {
  server_connections++;

  if (server_connections === target_connections)
    server.close();
}


function onExit() {
  var end = Date.now(),
      s = (end - start) / 1000,
      persec = Math.round(target_connections / s);

  assert.equal(initiated_connections, target_connections);
  assert.equal(client_connections, target_connections);
  assert.equal(server_connections, target_connections);

  console.log('%d connections in %d s', target_connections, s);
  console.log('%d connections per second', persec);
}
