const cluster = require('cluster');
if (cluster.isParent) {
  cluster.fork(); // one child
  cluster.on('exit', function(worker, code, signal) {
    console.log(`worker terminated with code ${code}`);
  });
}
