'use strict';

const myModule = process.argv[2];
if (!['http', 'https', 'http2'].includes(myModule)) {
  throw new Error(`Invalid module for benchmark test double: ${myModule}`);
}

let options;
if (myModule === 'https') {
  options = { rejectUnauthorized: false };
}

const http = require(myModule);

const duration = +process.env.duration;
const url = process.env.test_url;

const start = process.hrtime();
let throughput = 0;

function request(res, client) {
  res.resume();
  res.on('error', () => {});
  res.on('end', () => {
    throughput++;
    const [sec, nanosec] = process.hrtime(start);
    const ms = sec * 1000 + nanosec / 1e6;
    if (ms < duration * 1000) {
      run();
    } else {
      console.log(JSON.stringify({ throughput }));
      if (client) {
        client.destroy();
        process.exit(0);
      }
    }
  });
}

function run() {
  if (http.get) { // HTTP or HTTPS
    if (options) {
      http.get(url, options, request);
    } else {
      http.get(url, request);
    }
  } else { // HTTP/2
    const client = http.connect(url);
    client.on('error', () => {});
    request(client.request(), client);
  }
}

run();
