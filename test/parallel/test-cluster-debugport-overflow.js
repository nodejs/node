var spawn = require('child_process').spawn;
var cluster = require('cluster');
var assert = require('assert');

if (process.argv[2] == 'master') {
   if (cluster.isMaster) {
      cluster.fork().on('exit', function(code) {
         process.exit(code);
      });
   } else {
      process.exit(42);
   }
} else {
   // iojs --debug-port=65535 test-cluster-debugport-overflow.js master
   var args = ['--debug-port=65535', __filename, 'master'];
   spawn(process.argv[0], args).on('close', function(code) {
      assert.equal(42, code, 'Worker was started');
   });
}
