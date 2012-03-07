// first start node http_simple.js
var http = require('http');

var latency = [];

var numRequests = parseInt(process.argv[2], 10) || 100;
var maxSockets = parseInt(process.argv[3], 10) || 100;
var runs = parseInt(process.argv[4], 10) || 100;
var prefix = process.argv[5] || '';
if (prefix) prefix += '_';
var r = 0;

var port = parseInt(process.env.PORT, 10) || 8000;
var host = process.env.HOST || '127.0.0.1';

http.globalAgent.maxSockets = maxSockets;

run();

function run() {
  if (r++ === runs) {
    return finish();
  }

  // make numRequests in parallel
  // retain the order in which they are *made*.  This requires trapping
  // each one in a closure, since otherwise, we'll of course end
  // up mostly sorting them in ascending order, since the cb from a
  // fast request will almost always be called before the cb from a
  // slow one.
  var c = numRequests;
  var lat = [];
  latency.push(lat);
  for (var i = 0; i < numRequests; i++) (function (i) {
    makeRequest(function(l) {
      lat[i] = l;
      c--;
      if (c === 0) run();
    });
  })(i);
}

function makeRequest(cb) {
  var opts = { host: host,
               port: port,
               uri: 'http://'+host+':'+port+'/bytes/12',
               forever: true,
               path: '/bytes/12' };
  var pre = Date.now();
  var req = http.get(opts, function(res) {
    return cb(Date.now() - pre);
  });
}

function finish() {
  var data = [];
  latency.forEach(function(run, i) {
    run.forEach(function(l, j) {
      data[j] = data[j] || [];
      data[j][i] = l;
    });
  });

  data = data.map(function (l, i) {
    return l.join('\t')
  }).join('\n') + '\n';

  var fname = prefix +
              'client_latency_' +
              numRequests + '_' +
              maxSockets + '_' +
              runs + '.tab';
  var path = require('path');
  fname = path.resolve(__dirname, '..', 'out', fname);
  fname = path.relative(process.cwd(), fname);
  require('fs').writeFile(fname, data, function(er) {
    if (er) throw er;
    console.log('written: %s', fname);
  });
}
