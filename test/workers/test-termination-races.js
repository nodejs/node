// Flags: --experimental-workers
'use strict';

var common = require('../common');
common.use();
var assert = require('assert');
var Worker = require('worker');
var checks = 0;
var MESSAGES_PER_GRAND_CHILD_WORKER = 20;


if (process.isMainThread) {
  var l = 4;
  var workers = {};
  while (l--) {
    var worker = new Worker(__filename);
    worker.on('message', function(worker) {
      return function(data) {
        if (workers[data.id]) return;
        checks++;
        worker.terminate();
        workers[data.id] = true;
      };
    }(worker));
    worker.postMessage({id: l});
  }
  process.on('beforeExit', function() {
    assert.equal(4, checks);
  });
} else {
  Worker.on('message', function(data) {
    if (data.id <= 3) {
      runImmediateChildWorker(data);
    } else {
      runGrandChildWorker(data);
    }
  });

  var runImmediateChildWorker = function(mainData) {
    var messages = {};
    var l = 4;
    while (l--) {
      var subWorkerId = mainData.id * 4 + 4 + l;
      messages[subWorkerId] = 0;
      var worker = new Worker(__filename);
      worker.on('message', function(data) {
        var count = ++messages[data.id];
        if (count === MESSAGES_PER_GRAND_CHILD_WORKER) {
          Worker.postMessage({id: mainData.id});
        }
      });
      worker.postMessage({id: subWorkerId});
    }
  };

  var runGrandChildWorker = function(data) {
    var l = MESSAGES_PER_GRAND_CHILD_WORKER;
    process.stdout;
    process.stderr;
    process.stdin;
    try {require('assert');} catch (e) {}
    try {require('buffer');} catch (e) {}
    try {require('child_process');} catch (e) {}
    try {require('cluster');} catch (e) {}
    try {require('console');} catch (e) {}
    try {require('constants');} catch (e) {}
    try {require('crypto');} catch (e) {}
    try {require('_debug_agent');} catch (e) {}
    try {require('_debugger');} catch (e) {}
    try {require('dgram');} catch (e) {}
    try {require('dns');} catch (e) {}
    try {require('domain');} catch (e) {}
    try {require('events');} catch (e) {}
    try {require('freelist');} catch (e) {}
    try {require('fs');} catch (e) {}
    try {require('_http_agent');} catch (e) {}
    try {require('_http_client');} catch (e) {}
    try {require('_http_common');} catch (e) {}
    try {require('_http_incoming');} catch (e) {}
    try {require('http');} catch (e) {}
    try {require('_http_outgoing');} catch (e) {}
    try {require('_http_server');} catch (e) {}
    try {require('https');} catch (e) {}
    try {require('_linklist');} catch (e) {}
    try {require('module');} catch (e) {}
    try {require('net');} catch (e) {}
    try {require('os');} catch (e) {}
    try {require('path');} catch (e) {}
    try {require('process');} catch (e) {}
    try {require('punycode');} catch (e) {}
    try {require('querystring');} catch (e) {}
    try {require('readline');} catch (e) {}
    try {require('repl');} catch (e) {}
    try {require('smalloc');} catch (e) {}
    try {require('_stream_duplex');} catch (e) {}
    try {require('stream');} catch (e) {}
    try {require('_stream_passthrough');} catch (e) {}
    try {require('_stream_readable');} catch (e) {}
    try {require('_stream_transform');} catch (e) {}
    try {require('_stream_wrap');} catch (e) {}
    try {require('_stream_writable');} catch (e) {}
    try {require('string_decoder');} catch (e) {}
    try {require('timers');} catch (e) {}
    try {require('_tls_common');} catch (e) {}
    try {require('tls');} catch (e) {}
    try {require('_tls_legacy');} catch (e) {}
    try {require('_tls_wrap');} catch (e) {}
    try {require('tty');} catch (e) {}
    try {require('url');} catch (e) {}
    try {require('util');} catch (e) {}
    try {require('v8');} catch (e) {}
    try {require('vm');} catch (e) {}
    try {require('worker');} catch (e) {}
    try {require('zlib');} catch (e) {}
    while (l--) {
      Worker.postMessage({
        id: data.id
      });
    }
  };

}
