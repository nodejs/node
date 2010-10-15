net = require('net');

var errors = 0, connections = 0;

function connect () {
  process.nextTick(function () {
    var s = net.Stream();
    var gotConnected = false;
    s.connect(9000);
    s.on('connect', function () {
      gotConnected = true;
      connections++;
      connect();
    });

    var haderror = false;

    s.on('close', function () {
      if (gotConnected) connections--;
      if (!haderror) connect();
    });

    s.on('end', function () {
      s.end();
    });

    s.on('error', function () {
      haderror = true;
      errors++;
    });
  });
}

connect();


var oldConnections, oldErrors;

setInterval(function () {
  if (oldConnections != connections) {
    oldConnections = connections;
    console.log("CLIENT %d connections: %d", process.pid, connections);
  }

  if (oldErrors != errors) {
    oldErrors = errors;
    console.log("CLIENT %d errors: %d", process.pid, errors);
  }
}, 1000);

