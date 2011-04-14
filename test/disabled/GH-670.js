var assert = require('assert');
var https = require('https');
var tls = require('tls');

var options = {
  host: 'github.com',
  path: '/kriskowal/tigerblood/',
  port: 443
};

var req = https.get(options, function(response) {
  var recved = 0;

  response.on('data', function(chunk) {
    recved += chunk.length;
    console.log('Response data.');
  });

  response.on('end', function() {
    console.log('Response end.');
    // Does not work
    loadDom();
  });

});

req.on('error', function(e) {
  console.log('Error on get.');
});

function loadDom() {
  // Do a lot of computation to stall the process.
  // In the meantime the socket will be disconnected.
  for (var i = 0; i < 1e8; i++) {
    ;
  }

  console.log('Dom loaded.');
}
