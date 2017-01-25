'use strict';
// Regression test for GH-784
// https://github.com/joyent/node/issues/784
//
// The test works by making a total of 8 requests to the server.  The first
// two are made with the server off - they should come back as ECONNREFUSED.
// The next two are made with server on - they should come back successful.
// The next two are made with the server off - and so on.  Without the fix
// we were experiencing parse errors and instead of ECONNREFUSED.
const common = require('../common');
const http = require('http');
const assert = require('assert');


const server = http.createServer(function(req, res) {
  let body = '';

  req.setEncoding('utf8');
  req.on('data', function(chunk) {
    body += chunk;
  });

  req.on('end', function() {
    assert.equal('PING', body);
    res.writeHead(200);
    res.end('PONG');
  });
});


server.on('listening', pingping);


function serverOn() {
  console.error('Server ON');
  server.listen(common.PORT);
}


function serverOff() {
  console.error('Server OFF');
  server.close();
  pingping();
}

const responses = [];


function afterPing(result) {
  responses.push(result);
  console.error('afterPing. responses.length = ' + responses.length);
  switch (responses.length) {
    case 2:
      assert.ok(/ECONNREFUSED/.test(responses[0]));
      assert.ok(/ECONNREFUSED/.test(responses[1]));
      serverOn();
      break;

    case 4:
      assert.ok(/success/.test(responses[2]));
      assert.ok(/success/.test(responses[3]));
      serverOff();
      break;

    case 6:
      assert.ok(/ECONNREFUSED/.test(responses[4]));
      assert.ok(/ECONNREFUSED/.test(responses[5]));
      serverOn();
      break;

    case 8:
      assert.ok(/success/.test(responses[6]));
      assert.ok(/success/.test(responses[7]));
      server.close();
      // we should go to process.on('exit') from here.
      break;
  }
}


function ping() {
  console.error('making req');

  const opt = {
    port: common.PORT,
    path: '/ping',
    method: 'POST'
  };

  const req = http.request(opt, function(res) {
    let body = '';

    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      body += chunk;
    });

    res.on('end', function() {
      assert.equal('PONG', body);
      assert.ok(!hadError);
      gotEnd = true;
      afterPing('success');
    });
  });

  req.end('PING');

  let gotEnd = false;
  let hadError = false;

  req.on('error', function(error) {
    console.log('Error making ping req: ' + error);
    hadError = true;
    assert.ok(!gotEnd);
    afterPing(error.message);
  });
}


function pingping() {
  ping();
  ping();
}

pingping();

process.on('exit', function() {
  console.error("process.on('exit')");
  console.error(responses);

  assert.equal(8, responses.length);
});
