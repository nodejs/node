'use strict';

const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);

function inParentProcess(pid) {
  process.on('exit', () => {
    process.kill(pid);
  });

  function retry(fn, times) {
    return Promise.resolve()
      .then(
        () => fn(),
        (err) => {
          if (times > 0) {
            return retry(fn, times - 1);
          }
          throw err;
        }
      );
  }
  function request() {
    return new Promise((resolve, reject) => {
      require('http').request({
        socketPath: './http.sock',
        path: '/',
      }, (res) => {
        let data = '';
        res.setEncoding('utf-8');
        res.on('data', (chunk) => data += chunk);
        res.on('end', () => resolve(data));
        res.on('error', reject);
      }).setTimeout(100).on('error', reject).end();
    });
  }
  retry(request, 3)
    .then(common.mustCall((data) => assert.strictEqual(data, 'hello world')))
    .catch((err) => {
      console.error(err);
      process.exitCode = 1;
    });
}

function inChildProcess() {
  require('http')
    .createServer((req, res) => {
      console.log('on req');
      res.write('hello world');
      res.end();
    })
    .listen('./http.sock', () => {
      console.log('server started');
    });
}

const pid = binding.fork();
assert(pid >= 0);
switch (true) {
  case pid === 0: inChildProcess(); break;
  case pid > 0: inParentProcess(pid); break;
}
