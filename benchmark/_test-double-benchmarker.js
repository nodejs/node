'use strict';

const http = require('http');

const duration = process.env.duration || 0;
const url = process.env.test_url;

const start = process.hrtime();
let throughput = 0;

function request(res) {
  res.on('data', () => {});
  res.on('error', () => {});
  res.on('end', () => {
    throughput++;
    const diff = process.hrtime(start);
    if (duration > 0 && diff[0] < duration) {
      run();
    } else {
      console.log(JSON.stringify({ throughput }));
    }
  });
}

function run() {
  http.get(url, request);
}

run();
