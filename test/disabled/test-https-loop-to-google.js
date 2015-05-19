'use strict';
// Failing test for https

// Will fail with "socket hang up" for 4 out of 10 requests
// Tested on node 0.5.0-pre commit 9851574


var common = require('../common');
var https = require('https');

for (var i = 0; i < 10; ++i) {
  https.get({
    host: 'www.google.com',
    path: '/accounts/o8/id',
    port: 443
  }, function(res) {
    var data = '';
    res.on('data', function(chunk) {
      data += chunk;
    });
    res.on('end', function() {
      console.log(res.statusCode);
    });
  }).on('error', function(error) {
    console.log(error);
  });
}
