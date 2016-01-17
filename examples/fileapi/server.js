var WebSocketServer = require('../../').Server
  , express = require('express')
  , fs = require('fs')
  , http = require('http')
  , util = require('util')
  , path = require('path')
  , app = express.createServer()
  , events = require('events')
  , ansi = require('ansi')
  , cursor = ansi(process.stdout);

function BandwidthSampler(ws, interval) {
  interval = interval || 2000;
  var previousByteCount = 0;
  var self = this;
  var intervalId = setInterval(function() {
    var byteCount = ws.bytesReceived;
    var bytesPerSec = (byteCount - previousByteCount) / (interval / 1000);
    previousByteCount = byteCount;
    self.emit('sample', bytesPerSec);
  }, interval);
  ws.on('close', function() {
    clearInterval(intervalId);
  });
}
util.inherits(BandwidthSampler, events.EventEmitter);

function makePathForFile(filePath, prefix, cb) {
  if (typeof cb !== 'function') throw new Error('callback is required');
  filePath = path.dirname(path.normalize(filePath)).replace(/^(\/|\\)+/, '');
  var pieces = filePath.split(/(\\|\/)/);
  var incrementalPath = prefix;
  function step(error) {
    if (error) return cb(error);
    if (pieces.length == 0) return cb(null, incrementalPath);
    incrementalPath += '/' + pieces.shift();
    fs.exists(incrementalPath, function(exists) {
      if (!exists) fs.mkdir(incrementalPath, step);
      else process.nextTick(step);
    });
  }
  step();
}

cursor.eraseData(2).goto(1, 1);
app.use(express.static(__dirname + '/public'));

var clientId = 0;
var wss = new WebSocketServer({server: app});
wss.on('connection', function(ws) {
  var thisId = ++clientId;
  cursor.goto(1, 4 + thisId).eraseLine();
  console.log('Client #%d connected', thisId);

  var sampler = new BandwidthSampler(ws);
  sampler.on('sample', function(bps) {
    cursor.goto(1, 4 + thisId).eraseLine();
    console.log('WebSocket #%d incoming bandwidth: %d MB/s', thisId, Math.round(bps / (1024*1024)));
  });

  var filesReceived = 0;
  var currentFile = null;
  ws.on('message', function(data, flags) {
    if (!flags.binary) {
      currentFile = JSON.parse(data);
      // note: a real-world app would want to sanity check the data
    }
    else {
      if (currentFile == null) return;
      makePathForFile(currentFile.path, __dirname + '/uploaded', function(error, path) {
        if (error) {
          console.log(error);
          ws.send(JSON.stringify({event: 'error', path: currentFile.path, message: error.message}));
          return;
        }
        fs.writeFile(path + '/' + currentFile.name, data, function(error) {
          ++filesReceived;
          // console.log('received %d bytes long file, %s', data.length, currentFile.path);
          ws.send(JSON.stringify({event: 'complete', path: currentFile.path}));
          currentFile = null;
        });
      });
    }
  });

  ws.on('close', function() {
    cursor.goto(1, 4 + thisId).eraseLine();
    console.log('Client #%d disconnected. %d files received.', thisId, filesReceived);
  });

  ws.on('error', function(e) {
    cursor.goto(1, 4 + thisId).eraseLine();
    console.log('Client #%d error: %s', thisId, e.message);
  });
});

fs.mkdir(__dirname + '/uploaded', function(error) {
  // ignore errors, most likely means directory exists
  console.log('Uploaded files will be saved to %s/uploaded.', __dirname);
  console.log('Remember to wipe this directory if you upload lots and lots.');
  app.listen(8080);
  console.log('Listening on http://localhost:8080');
});
