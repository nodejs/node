'use strict';
var common = require('../common');
var assert = require('assert');

var Readable = require('stream').Readable;

test1();
test2();

function test1() {
  var r = new Readable();

  // should not end when we get a Buffer(0) or '' as the _read result
  // that just means that there is *temporarily* no data, but to go
  // ahead and try again later.
  //
  // note that this is very unusual.  it only works for crypto streams
  // because the other side of the stream will call read(0) to cycle
  // data through openssl.  that's why we set the timeouts to call
  // r.read(0) again later, otherwise there is no more work being done
  // and the process just exits.

  var buf = new Buffer(5);
  buf.fill('x');
  var reads = 5;
  r._read = function(n) {
    switch (reads--) {
      case 0:
        return r.push(null); // EOF
      case 1:
        return r.push(buf);
      case 2:
        setTimeout(r.read.bind(r, 0), 50);
        return r.push(new Buffer(0)); // Not-EOF!
      case 3:
        setTimeout(r.read.bind(r, 0), 50);
        return process.nextTick(function() {
          return r.push(new Buffer(0));
        });
      case 4:
        setTimeout(r.read.bind(r, 0), 50);
        return setTimeout(function() {
          return r.push(new Buffer(0));
        });
      case 5:
        return setTimeout(function() {
          return r.push(buf);
        });
      default:
        throw new Error('unreachable');
    }
  };

  var results = [];
  function flow() {
    var chunk;
    while (null !== (chunk = r.read()))
      results.push(chunk + '');
  }
  r.on('readable', flow);
  r.on('end', function() {
    results.push('EOF');
  });
  flow();

  process.on('exit', function() {
    assert.deepEqual(results, [ 'xxxxx', 'xxxxx', 'EOF' ]);
    console.log('ok');
  });
}

function test2() {
  var r = new Readable({ encoding: 'base64' });
  var reads = 5;
  r._read = function(n) {
    if (!reads--)
      return r.push(null); // EOF
    else
      return r.push(new Buffer('x'));
  };

  var results = [];
  function flow() {
    var chunk;
    while (null !== (chunk = r.read()))
      results.push(chunk + '');
  }
  r.on('readable', flow);
  r.on('end', function() {
    results.push('EOF');
  });
  flow();

  process.on('exit', function() {
    assert.deepEqual(results, [ 'eHh4', 'eHg=', 'EOF' ]);
    console.log('ok');
  });
}
