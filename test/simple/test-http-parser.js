var common = require('../common');
var assert = require('assert');

// The purpose of this test is not to check HTTP compliance but to test the
// binding. Tests for pathological http messages should be submitted
// upstream to http://github.com/ry/http-parser for inclusion into
// deps/http-parser/test.c

var HTTPParser = process.binding('http_parser').HTTPParser;

var parser = new HTTPParser('request');

var Buffer = require('buffer').Buffer;
var buffer = new Buffer(1024);

var request = 'GET /hello HTTP/1.1\r\n\r\n';

buffer.write(request, 0, 'ascii');

var callbacks = 0;

parser.onMessageBegin = function() {
  console.log('message begin');
  callbacks++;
};

parser.onHeadersComplete = function(info) {
  console.log('headers complete: ' + JSON.stringify(info));
  assert.equal('GET', info.method);
  assert.equal(1, info.versionMajor);
  assert.equal(1, info.versionMinor);
  callbacks++;
};

parser.onURL = function(b, off, len) {
  //throw new Error('hello world');
  callbacks++;
};

parser.onPath = function(b, off, length) {
  console.log('path [' + off + ', ' + length + ']');
  var path = b.toString('ascii', off, off + length);
  console.log('path = "' + path + '"');
  assert.equal('/hello', path);
  callbacks++;
};

parser.execute(buffer, 0, request.length);
assert.equal(4, callbacks);

//
// Check that if we throw an error in the callbacks that error will be
// thrown from parser.execute()
//

parser.onURL = function(b, off, len) {
  throw new Error('hello world');
};

assert.throws(function() {
  parser.execute(buffer, 0, request.length);
}, Error, 'hello world');

