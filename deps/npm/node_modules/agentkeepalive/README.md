# agentkeepalive

[![NPM version][npm-image]][npm-url]
[![build status][travis-image]][travis-url]
[![Appveyor status][appveyor-image]][appveyor-url]
[![Test coverage][codecov-image]][codecov-url]
[![David deps][david-image]][david-url]
[![Known Vulnerabilities][snyk-image]][snyk-url]
[![npm download][download-image]][download-url]

[npm-image]: https://img.shields.io/npm/v/agentkeepalive.svg?style=flat
[npm-url]: https://npmjs.org/package/agentkeepalive
[travis-image]: https://img.shields.io/travis/node-modules/agentkeepalive.svg?style=flat
[travis-url]: https://travis-ci.org/node-modules/agentkeepalive
[appveyor-image]: https://ci.appveyor.com/api/projects/status/k7ct4s47di6m5uy2?svg=true
[appveyor-url]: https://ci.appveyor.com/project/fengmk2/agentkeepalive
[codecov-image]: https://codecov.io/gh/node-modules/agentkeepalive/branch/master/graph/badge.svg
[codecov-url]: https://codecov.io/gh/node-modules/agentkeepalive
[david-image]: https://img.shields.io/david/node-modules/agentkeepalive.svg?style=flat
[david-url]: https://david-dm.org/node-modules/agentkeepalive
[snyk-image]: https://snyk.io/test/npm/agentkeepalive/badge.svg?style=flat-square
[snyk-url]: https://snyk.io/test/npm/agentkeepalive
[download-image]: https://img.shields.io/npm/dm/agentkeepalive.svg?style=flat-square
[download-url]: https://npmjs.org/package/agentkeepalive

The Node.js's missing `keep alive` `http.Agent`. Support `http` and `https`.

## What's different from original `http.Agent`?

- `keepAlive=true` by default
- Disable Nagle's algorithm: `socket.setNoDelay(true)`
- Add free socket timeout: avoid long time inactivity socket leak in the free-sockets queue.
- Add active socket timeout: avoid long time inactivity socket leak in the active-sockets queue.

## Install

```bash
$ npm install agentkeepalive --save
```

## new Agent([options])

* `options` {Object} Set of configurable options to set on the agent.
  Can have the following fields:
  * `keepAlive` {Boolean} Keep sockets around in a pool to be used by
    other requests in the future. Default = `true`.
  * `keepAliveMsecs` {Number} When using the keepAlive option, specifies the initial delay
    for TCP Keep-Alive packets. Ignored when the keepAlive option is false or undefined. Defaults to 1000.
    Default = `1000`.  Only relevant if `keepAlive` is set to `true`.
  * `freeSocketKeepAliveTimeout`: {Number} Sets the free socket to timeout
    after `freeSocketKeepAliveTimeout` milliseconds of inactivity on the free socket.
    Default is `15000`.
    Only relevant if `keepAlive` is set to `true`.
  * `timeout`: {Number} Sets the working socket to timeout
    after `timeout` milliseconds of inactivity on the working socket.
    Default is `freeSocketKeepAliveTimeout * 2`.
  * `maxSockets` {Number} Maximum number of sockets to allow per
    host. Default = `Infinity`.
  * `maxFreeSockets` {Number} Maximum number of sockets (per host) to leave open
    in a free state. Only relevant if `keepAlive` is set to `true`.
    Default = `256`.
  * `socketActiveTTL` {Number} Sets the socket active time to live, even if it's in use.
    If not setted the behaviour continues the same (the socket will be released only when free)
    Default = `null`.

## Usage

```js
const http = require('http');
const Agent = require('agentkeepalive');

const keepaliveAgent = new Agent({
  maxSockets: 100,
  maxFreeSockets: 10,
  timeout: 60000,
  freeSocketKeepAliveTimeout: 30000, // free socket keepalive for 30 seconds
});

const options = {
  host: 'cnodejs.org',
  port: 80,
  path: '/',
  method: 'GET',
  agent: keepaliveAgent,
};

const req = http.request(options, res => {
  console.log('STATUS: ' + res.statusCode);
  console.log('HEADERS: ' + JSON.stringify(res.headers));
  res.setEncoding('utf8');
  res.on('data', function (chunk) {
    console.log('BODY: ' + chunk);
  });
});
req.on('error', e => {
  console.log('problem with request: ' + e.message);
});
req.end();

setTimeout(() => {
  if (keepaliveAgent.statusChanged) {
    console.log('[%s] agent status changed: %j', Date(), keepaliveAgent.getCurrentStatus());
  }
}, 2000);

```

### `getter agent.statusChanged`

counters have change or not after last checkpoint.

### `agent.getCurrentStatus()`

`agent.getCurrentStatus()` will return a object to show the status of this agent:

```js
{
  createSocketCount: 10,
  closeSocketCount: 5,
  timeoutSocketCount: 0,
  requestCount: 5,
  freeSockets: { 'localhost:57479:': 3 },
  sockets: { 'localhost:57479:': 5 },
  requests: {}
}
```

### Support `https`

```js
const https = require('https');
const HttpsAgent = require('agentkeepalive').HttpsAgent;

const keepaliveAgent = new HttpsAgent();
// https://www.google.com/search?q=nodejs&sugexp=chrome,mod=12&sourceid=chrome&ie=UTF-8
const options = {
  host: 'www.google.com',
  port: 443,
  path: '/search?q=nodejs&sugexp=chrome,mod=12&sourceid=chrome&ie=UTF-8',
  method: 'GET',
  agent: keepaliveAgent,
};

const req = https.request(options, res => {
  console.log('STATUS: ' + res.statusCode);
  console.log('HEADERS: ' + JSON.stringify(res.headers));
  res.setEncoding('utf8');
  res.on('data', chunk => {
    console.log('BODY: ' + chunk);
  });
});

req.on('error', e => {
  console.log('problem with request: ' + e.message);
});
req.end();

setTimeout(() => {
  console.log('agent status: %j', keepaliveAgent.getCurrentStatus());
}, 2000);
```

## [Benchmark](https://github.com/node-modules/agentkeepalive/tree/master/benchmark)

run the benchmark:

```bash
cd benchmark
sh start.sh
```

Intel(R) Core(TM)2 Duo CPU     P8600  @ 2.40GHz

node@v0.8.9

50 maxSockets, 60 concurrent, 1000 requests per concurrent, 5ms delay

Keep alive agent (30 seconds):

```js
Transactions:          60000 hits
Availability:         100.00 %
Elapsed time:          29.70 secs
Data transferred:        14.88 MB
Response time:            0.03 secs
Transaction rate:      2020.20 trans/sec
Throughput:           0.50 MB/sec
Concurrency:           59.84
Successful transactions:       60000
Failed transactions:             0
Longest transaction:          0.15
Shortest transaction:         0.01
```

Normal agent:

```js
Transactions:          60000 hits
Availability:         100.00 %
Elapsed time:          46.53 secs
Data transferred:        14.88 MB
Response time:            0.05 secs
Transaction rate:      1289.49 trans/sec
Throughput:           0.32 MB/sec
Concurrency:           59.81
Successful transactions:       60000
Failed transactions:             0
Longest transaction:          0.45
Shortest transaction:         0.00
```

Socket created:

```
[proxy.js:120000] keepalive, 50 created, 60000 requestFinished, 1200 req/socket, 0 requests, 0 sockets, 0 unusedSockets, 50 timeout
{" <10ms":662," <15ms":17825," <20ms":20552," <30ms":17646," <40ms":2315," <50ms":567," <100ms":377," <150ms":56," <200ms":0," >=200ms+":0}
----------------------------------------------------------------
[proxy.js:120000] normal   , 53866 created, 84260 requestFinished, 1.56 req/socket, 0 requests, 0 sockets
{" <10ms":75," <15ms":1112," <20ms":10947," <30ms":32130," <40ms":8228," <50ms":3002," <100ms":4274," <150ms":181," <200ms":18," >=200ms+":33}
```

## License

```
(The MIT License)

Copyright(c) node-modules and other contributors.
Copyright(c) 2012 - 2015 fengmk2 <fengmk2@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
'Software'), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
