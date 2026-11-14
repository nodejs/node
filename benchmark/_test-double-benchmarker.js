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
const { createHistogram } = require('perf_hooks');

const duration = +process.env.duration;
const url = process.env.test_url;

const start = process.hrtime();
let throughput = 0;
const latencies = createHistogram();
let requestStart;

function latencyInMs() {
  if (latencies.count === 0) {
    return undefined;
  }
  const toMs = (ns) => ns / 1e6;
  return {
    p50: toMs(latencies.percentile(50)),
    p90: toMs(latencies.percentile(90)),
    p99: toMs(latencies.percentile(99)),
    p99_9: toMs(latencies.percentile(99.9)),
  };
}

function request(res, client) {
  res.resume();
  res.on('error', () => {});
  res.on('end', () => {
    throughput++;
    latencies.record(process.hrtime.bigint() - requestStart);
    const [sec, nanosec] = process.hrtime(start);
    const ms = sec * 1000 + nanosec / 1e6;
    if (ms < duration * 1000) {
      run();
    } else {
      console.log(JSON.stringify({ throughput, latency: latencyInMs() }));
      if (client) {
        client.destroy();
        process.exit(0);
      }
    }
  });
}

function run() {
  requestStart = process.hrtime.bigint();
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
