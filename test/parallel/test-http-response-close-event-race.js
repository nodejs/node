var common = require('../common');
var assert = require('assert');
var http = require('http');

var clientRequest = null;
var eventCount = 0;
var testTickCount = 3;

var server = http.createServer(function(req, res) {
  console.log('server: request');

  res.on('finish', function() {
    console.log('server: response finish');
    eventCount++;
  });
  res.on('close', function() {
    console.log('server: response close');
    eventCount++;
  });

  console.log('client: aborting request');
  clientRequest.abort();

  var ticks = 0;
  function tick() {
    console.log('server: tick ' + ticks +
                (req.connection.destroyed ? ' (connection destroyed!)' : ''));

    if (ticks < testTickCount) {
      ticks++;
      setImmediate(tick);
    } else {
      sendResponse();
    }
  }
  tick();

  function sendResponse() {
    console.log('server: sending response');
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.end('Response\n');
    console.log('server: res.end() returned');

    handleResponseEnd();
  }
});

server.on('listening', function() {
  console.log('server: listening on port ' + common.PORT);
  console.log('-----------------------------------------------------');
  startRequest();
});

server.on('connection', function(connection) {
  console.log('server: connection');
  connection.on('close', function() {
    console.log('server: connection close');
  });
});

server.on('close', function() {
  console.log('server: close');
});

server.listen(common.PORT);

function startRequest() {
  console.log('client: starting request - testing with %d ticks after abort()',
              testTickCount);
  eventCount = 0;

  var options = {port: common.PORT, path: '/'};
  clientRequest = http.get(options, function() {});
  clientRequest.on('error', function() {});
}

function handleResponseEnd() {
  setImmediate(function() {
    setImmediate(function() {
      assert.equal(eventCount, 1);

      if (testTickCount > 0) {
        testTickCount--;
        startRequest();
      } else {
        server.close();
      }
    });
  });
}
