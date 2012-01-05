var isolates = process.binding('isolates');

var N = 4; // # of child isolates

if (process.tid === 1)
  master();
else
  child();

function master() {
  for (var i = 0; i < N; ++i) spawn();

  function spawn() {
    var isolate = isolates.create(process.argv);
    isolate.onexit = function() {
      console.error("onexit isolate #%d", isolate.tid);
    };
    isolate.onmessage = function(m) {
      console.error("parent received message '%s'", m);
      isolate.send(Buffer('ACK ' + m));
    };
  }
}

function child() {
  var n = 0;

  function send() {
    if (++n > 10) return;
    process._send(Buffer('SYN' + n));
    setTimeout(send, 10);
  }

  send();

  process._onmessage = function(m) {
    console.error("child %d received message '%s'", process.tid, m);
  };
}
