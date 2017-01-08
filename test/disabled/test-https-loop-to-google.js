'use strict';
// Failing test for https

// Will fail with "socket hang up" for 4 out of 10 requests
// Tested on node 0.5.0-pre commit 9851574


const common = require('../common');
const https = require('https');

for (let i = 0; i < 10; ++i) {
  https.get({
    host: 'www.google.com',
    path: '/accounts/o8/id',
    port: 443
  }, (res) => {
    let data = '';
    res.on('data', (chunk) => {
      data += chunk;
    });
    res.on('end', () => {
      console.log(res.statusCode);
    });
  }).on('error', (error) => {
    console.log(error);
  });
}
