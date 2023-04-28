'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const Countdown = require('../common/countdown');

function explicit(req, res) {
  assert.throws(() => {
    res.writeHead(200, 'OK\r\nContent-Type: text/html\r\n');
  }, /Invalid character in statusMessage/);

  assert.throws(() => {
    res.writeHead(200, 'OK\u010D\u010AContent-Type: gotcha\r\n');
  }, /Invalid character in statusMessage/);

  res.statusMessage = 'OK';
  res.end();
}

function implicit(req, res) {
  assert.throws(() => {
    res.statusMessage = 'OK\r\nContent-Type: text/html\r\n';
    res.writeHead(200);
  }, /Invalid character in statusMessage/);
  res.statusMessage = 'OK';
  res.end();
}

const server = http.createServer((req, res) => {
  if (req.url === '/explicit') {
    explicit(req, res);
  } else {
    implicit(req, res);
  }
}).listen(0, common.mustCall(() => {
  const hostname = 'localhost';
  const countdown = new Countdown(2, () => server.close());
  const url = `http://${hostname}:${server.address().port}`;
  const check = common.mustCall((res) => {
    assert.notStrictEqual(res.headers['content-type'], 'text/html');
    assert.notStrictEqual(res.headers['content-type'], 'gotcha');
    countdown.dec();
  }, 2);
  http.get(`${url}/explicit`, check).end();
  http.get(`${url}/implicit`, check).end();
}));
