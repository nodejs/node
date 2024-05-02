'use strict';
const common = require('../common');
const { before, after, test } = require('node:test');
const { createServer } = require('node:http');

let server;

before(common.mustCall(() => {
  server = createServer();

  return new Promise(common.mustCall((resolve, reject) => {
    server.listen(0, common.mustCall((err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    }));
  }));
}));

after(common.mustCall(() => {
  server.close(common.mustCall());
}));

test();
