var isolates = process.binding('isolates');
var assert = require('assert');

var N_ISOLATES = 4;
var N_MESSAGES = 20;
var N_MESSAGES_PER_TICK = 4;

assert(N_MESSAGES % N_MESSAGES_PER_TICK == 0);

if (process.tid === 1)
  master();
else
  child();

function master() {
  for (var i = 0; i < N_ISOLATES; ++i) spawn();

  function spawn() {
    var isolate = isolates.create(process.argv);

    var gotExit = false; // exit event emitted?
    var msgId = 0; // message IDs seen so far
    var tick = 0;

    isolate.onexit = function() {
      gotExit = true;
    };

    isolate.onmessage = function(buf) {
      var msg = JSON.parse(buf);
      assert.equal(msg.id, msgId + 1); // verify that messages arrive in order
      assert.equal(msg.tick, tick); // and on the proper tick (=full mq drain)
      msgId = msg.id;
      if (msgId % N_MESSAGES_PER_TICK == 0) tick++;
      isolate.send(buf);
    };

    process.on('exit', function() {
      assert.equal(gotExit, true);
      assert.equal(msgId, N_MESSAGES);
      assert.equal(tick, N_MESSAGES / N_MESSAGES_PER_TICK);
    });
  }
}

function child() {
  var msgId = 0;
  var tick = 0;

  function send() {
    // Send multiple messages, verify that the message queue
    // is completely drained on each tick of the event loop.
    for (var i = 0; i < N_MESSAGES_PER_TICK; ++i) {
      process.send({tick:tick, id:++msgId});
    }

    if (msgId < N_MESSAGES) {
      setTimeout(send, 10);
    }

    tick++;
  }

  send();

  process._onmessage = function(m) {
  };
}
