var cluster = require('cluster');
cluster.isMaster || process.exit(42 + cluster.worker.id); // +42 to distinguish
// from exit(1) for other random reasons
