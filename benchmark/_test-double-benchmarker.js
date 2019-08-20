'use strict';

const myModule = process.argv[2];
if (!['http', 'http2'].includes(myModule)) {
  throw new Error(`Invalid module for benchmark test double: ${myModule}`);
}

const http = require(myModule);

const duration = process.env.duration || 0;
const url = process.env.test_url;

const start = process.hrtime();
let throughput = 0;

function request(res, client) {
  res.resume();
  res.on('error', () => {});
  res.on('end', () => {
    throughput++;
    const diff = process.hrtime(start);
    if (duration > 0 && diff[0] < duration) {
      run();
    } else {
      console.log(JSON.stringify({ throughput }));
      if (client) {
        client.destroy();
      }
    }
  });
}

function run() {
  if (http.get) { // HTTP
    http.get(url, request);
  } else {        // HTTP/2
    const client = http.connect(url);
    client.on('error', (e) => { throw e; });
    request(client.request(), client);
  }
}

run();
