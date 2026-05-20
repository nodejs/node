'use strict';
const common = require('../common.js');
const dc = require('diagnostics_channel');
const { AsyncLocalStorage } = require('async_hooks');
const http = require('http');

const bench = common.createBenchmark(main, {
  apm: ['none', 'diagnostics_channel', 'patch'],
  type: 'buffer',
  len: 1024,
  chunks: 4,
  connections: [50, 500],
  chunkedEnc: 1,
  duration: 5,
});

function main({ apm, connections, duration, type, len, chunks, chunkedEnc }) {
  const done = { none, patch, diagnostics_channel }[apm]();

  const server = require('../fixtures/simple-http-server.js')
    .listen(common.PORT)
    .on('listening', () => {
      const path = `/${type}/${len}/${chunks}/normal/${chunkedEnc}`;
      bench.http({
        path,
        connections,
        duration,
      }, () => {
        server.close();
        if (done) done();
      });
    });
}

function none() {}

function patch() {
  const als = new AsyncLocalStorage();
  const times = [];

  const { emit } = http.Server.prototype;
  function wrappedEmit(...args) {
    const [name, req, res] = args;
    if (name === 'request') {
      als.enterWith({
        url: req.url,
        start: process.hrtime.bigint(),
      });

      res.on('finish', () => {
        times.push({
          ...als.getStore(),
          statusCode: res.statusCode,
          end: process.hrtime.bigint(),
        });
      });
    }
    return emit.apply(this, args);
  }
  http.Server.prototype.emit = wrappedEmit;

  return () => {
    http.Server.prototype.emit = emit;
  };
}

function diagnostics_channel() {
  const als = new AsyncLocalStorage();
  const times = [];

  const start = dc.channel('http.server.request.start');
  const finish = dc.channel('http.server.response.finish');

  function onStart(req) {
    als.enterWith({
      url: req.url,
      start: process.hrtime.bigint(),
    });
  }

  function onFinish(res) {
    times.push({
      ...als.getStore(),
      statusCode: res.statusCode,
      end: process.hrtime.bigint(),
    });
  }

  start.subscribe(onStart);
  finish.subscribe(onFinish);

  return () => {
    start.unsubscribe(onStart);
    finish.unsubscribe(onFinish);
  };
}
