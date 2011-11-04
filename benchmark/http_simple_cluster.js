var cluster = require('cluster');
var os = require('os');

if (cluster.isMaster) {
  for (var i = 0, n = os.cpus().length; i < n; ++i) cluster.fork();
} else {
  require(__dirname + '/http_simple.js');
}
