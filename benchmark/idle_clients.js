net = require('net');

var errors = 0, connections = 0;

var lastClose = 0;

function maybeConnect (s) {
  var now = new Date();
  if (now - lastClose > 5000) {
    // Just connect immediately
    connect();
  } else {
    // Otherwise wait a little - see if this one is connected still. Just to
    // avoid spinning at 100% cpu when the server totally rejects our
    // connections.
    setTimeout(function () {
      if (s.writable && s.readable) connect();
    }, 100);
  }
}

function connect () {
  process.nextTick(function () {
    var s = net.Stream();
    var gotConnected = false;
    s.connect(9000);

    s.on('connect', function () {
      gotConnected = true;
      connections++;
      maybeConnect(s);
    });

    s.on('close', function () {
      if (gotConnected) connections--;
      lastClose = new Date();
    });

    s.on('error', function () {
      errors++;
    });
  });
}

connect();


var oldConnections, oldErrors;

// Try to start new connections every so often
setInterval(connect, 5000);

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

