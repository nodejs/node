common = require("../common");
assert = common.assert

var dgram = require("dgram"),
    sys = require('sys'),
    assert = require('assert'),
    Buffer = require("buffer").Buffer;
var timeoutTimer;
var LOCAL_BROADCAST_HOST = '224.0.0.1';
var sendMessages = [
  new Buffer("First message to send"),
  new Buffer("Second message to send"),
  new Buffer("Third message to send"),
  new Buffer("Fourth message to send")
];
var listenSockets = [];
var sendSocket = dgram.createSocket('udp4')
  .on('close', function () { console.log('sendSocket closed'); })
  .on('error', function (err) { throw err; });
sendSocket.setBroadcast(true);
var i = 0;
sendSocket.sendNext = function (){
  sendSocket.started = true;
  var buf = sendMessages[i++];
  if (!buf) {
    try { sendSocket.close(); }catch(e){}
    listenSockets.forEach(function (sock) { sock.close(); });
    clearTimeout(timeoutTimer);
    return;
  }
  sendSocket.send(buf, 0, buf.length, common.PORT, LOCAL_BROADCAST_HOST,
      function (err) {
    if (err) throw err;
    console.log('sent %s to %s', sys.inspect(buf.toString()),
      LOCAL_BROADCAST_HOST+common.PORT);
    process.nextTick(sendSocket.sendNext);
  });
}

function mkListener() {
  var receivedMessages = [];
  var listenSocket = dgram.createSocket('udp4')
    .on('message', function(buf, rinfo) {
      console.log('received %s from %j', sys.inspect(buf.toString()), rinfo);
      receivedMessages.push(buf);
    })
    .on('close', function () {
      console.log('listenSocket closed -- checking received messages');
      var count = 0;
      receivedMessages.forEach(function(buf){
        for (var i=0; i<sendMessages.length; ++i) {
          if (buf.toString() === sendMessages[i].toString()) {
            count++;
            break;
          }
        }
      });
      assert.strictEqual(count, sendMessages.length);
    })
    .on('error', function (err) { throw err; })
    .on('listening', function() {
      if (!sendSocket.started) {
        sendSocket.started = true;
        process.nextTick(function(){ sendSocket.sendNext(); });
      }
    })
  listenSocket.bind(common.PORT);
  listenSockets.push(listenSocket);
}

mkListener();
mkListener();
mkListener();

timeoutTimer = setTimeout(function () { throw new Error("Timeout"); }, 500);
