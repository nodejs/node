'use strict';
const common = require('../common'),
  assert = require('assert'),
  http = require('http');

var testResBody = 'other stuff!\n';

var server = http.createServer(function(req, res) {
  res.on('beforeFlushingHead', function(args) {
    args.statusCode = 201;
    args.statusMessage = 'changed to show we can';
    args.headers['Flush-Head'] = 'event-was-called';
  });
  res.writeHead(200, {
    'Content-Type': 'text/plain'
  });
  res.end(testResBody);
});
server.listen(common.PORT);


server.addListener('listening', function() {
  var options = {
    port: common.PORT,
    path: '/',
    method: 'GET'
  };
  var req = http.request(options, function(res) {
    assert.ok(res.statusCode === 201,
              'Response status code was not overridden from 200 to 201.');
    assert.ok(res.statusMessage === 'changed to show we can',
              'Response status message was not overridden.');
    assert.ok('flush-head' in res.headers,
              'Response headers didn\'t contain the flush-head header, ' +
                'indicating the beforeFlushingHead event was not called or ' +
                'did not allow adding headers.');
    assert.ok(res.headers['flush-head'] === 'event-was-called',
              'Response headers didn\'t contain the flush-head header ' +
                'with value event-was-called, indicating the ' +
                'beforeFlushingHead event was not called or did not allow ' +
                'adding headers.');
    res.addListener('end', function() {
      server.close();
      process.exit();
    });
    res.resume();
  });
  req.end();
});
