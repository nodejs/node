'use strict';

const http = require('http');

http.get(process.env.test_url, function() {
  console.log(JSON.stringify({ throughput: 1 }));
});
