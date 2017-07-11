'use strict';

const http = require('http');

http.get(process.env.path, function() {
  console.log(JSON.stringify({ throughput: 1 }));
});
