'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var path = require('path');
var net = require('net');

var isDone = false;
var targetPath = path.resolve(common.fixturesDir, 'debug-target.js');

var child = spawn(process.execPath, ['--debug-brk=' + common.PORT, targetPath]);
child.stderr.on('data', function() {
  child.emit('debug_start');
});

child.on('exit', function() {
  assert(isDone);
  console.log('ok');
});

child.once('debug_start', function() {
  // delayed for some time until debug agent is ready
  setTimeout(function() {
    debug_client_connect();
  }, 200);
});


function debug_client_connect() {
  var msg = null;
  var tmpBuf = '';

  var conn = net.connect({port: common.PORT});
  conn.setEncoding('utf8');
  conn.on('data', function(data) {
    tmpBuf += data;
    parse();
  });

  function parse() {
    if (!msg) {
      msg = {
        headers: null,
        contentLength: 0
      };
    }
    if (!msg.headers) {
      var offset = tmpBuf.indexOf('\r\n\r\n');
      if (offset < 0) return;
      msg.headers = tmpBuf.substring(0, offset);
      tmpBuf = tmpBuf.slice(offset + 4);
      var matches = /Content-Length: (\d+)/.exec(msg.headers);
      if (matches[1]) {
        msg.contentLength = +(matches[1]);
      }
    }
    if (msg.headers && Buffer.byteLength(tmpBuf) >= msg.contentLength) {
      try {
        var b = Buffer.from(tmpBuf);
        var body = b.toString('utf8', 0, msg.contentLength);
        tmpBuf = b.toString('utf8', msg.contentLength, b.length);

        // get breakpoint list and check if it exists on line 0
        if (!body.length) {
          var req = JSON.stringify({'seq': 1, 'type': 'request',
                                    'command': 'listbreakpoints'});
          conn.write('Content-Length: ' + req.length + '\r\n\r\n' + req);
          return;
        }

        var obj = JSON.parse(body);
        if (obj.type === 'response' && obj.command === 'listbreakpoints' &&
            !obj.running) {
          obj.body.breakpoints.forEach(function(bpoint) {
            if (bpoint.line === 0) isDone = true;
          });
        }

        var req = JSON.stringify({'seq': 100, 'type': 'request',
                                  'command': 'disconnect'});
        conn.write('Content-Length: ' + req.length + '\r\n\r\n' + req);
      } finally {
        msg = null;
        parse();
      }
    }
  }
}
