var WebSocket = require('../');
var currentTest = 1;
var lastTest = -1;
var testCount = null;

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

function nextTest() {
  if (currentTest > testCount || (lastTest != -1 && currentTest > lastTest)) {
    console.log('Updating reports and shutting down');
    var ws = new WebSocket('ws://localhost:9001/updateReports?agent=ws');
    ws.on('close', function() {
      process.exit();
    });
    return;
  };
  console.log('Running test case ' + currentTest + '/' + testCount);
  var ws = new WebSocket('ws://localhost:9001/runCase?case=' + currentTest + '&agent=ws');
  ws.on('message', function(data, flags) {
    ws.send(flags.buffer, {binary: flags.binary === true, mask: true});
  });
  ws.on('close', function(data) {
    currentTest += 1;
    process.nextTick(nextTest);
  });
  ws.on('error', function(e) {});
}

var ws = new WebSocket('ws://localhost:9001/getCaseCount');
ws.on('message', function(data, flags) {
  testCount = parseInt(data);
});
ws.on('close', function() {
  if (testCount > 0) {
    nextTest();
  }
});