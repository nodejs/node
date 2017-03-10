const cluster = require('cluster');
if (cluster.isMaster) {
  cluster.fork(); // one child
  cluster.on('exit', (worker, code, signal) => {
    console.log(`worker terminated with code ${code}`);
  });
}
