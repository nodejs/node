var WebSocketServer = require('../').Server;

process.on('uncaughtException', function(err) {
  console.log('Caught exception: ', err, err.stack);
});

process.on('SIGINT', function () {
  try {
    console.log('Updating reports and shutting down');
    var ws = new WebSocket('ws://localhost:9001/updateReports?agent=ws');
    ws.on('close', function() {
      process.exit();
    });
  }
  catch(e) {
    process.exit();
  }
});

var wss = new WebSocketServer({port: 8181});
wss.on('connection', function(ws) {
  console.log('new connection');
  ws.on('message', function(data, flags) {
    ws.send(flags.buffer, {binary: flags.binary === true});
  });
  ws.on('error', function() {
    console.log('error', arguments);
  });
});
