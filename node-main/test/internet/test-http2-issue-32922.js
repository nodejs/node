'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const net = require('net');

const {
  HTTP2_HEADER_PATH,
} = http2.constants;

// Create a normal session, as a control case
function normalSession(cb) {
  http2.connect('https://google.com', (clientSession) => {
    let error = null;
    const req = clientSession.request({ [HTTP2_HEADER_PATH]: '/' });
    req.on('error', (err) => {
      error = err;
    });
    req.on('response', (_headers) => {
      req.on('data', (_chunk) => { });
      req.on('end', () => {
        clientSession.close();
        return cb(error);
      });
    });
  });
}
normalSession(common.mustSucceed());

// Create a session using a socket that has not yet finished connecting
function socketNotFinished(done) {
  const socket2 = net.connect(443, 'google.com');
  http2.connect('https://google.com', { socket2 }, (clientSession) => {
    let error = null;
    const req = clientSession.request({ [HTTP2_HEADER_PATH]: '/' });
    req.on('error', (err) => {
      error = err;
    });
    req.on('response', (_headers) => {
      req.on('data', (_chunk) => { });
      req.on('end', () => {
        clientSession.close();
        socket2.destroy();
        return done(error);
      });
    });
  });
}
socketNotFinished(common.mustSucceed());

// Create a session using a socket that has finished connecting
function socketFinished(done) {
  const socket = net.connect(443, 'google.com', () => {
    http2.connect('https://google.com', { socket }, (clientSession) => {
      let error = null;
      const req = clientSession.request({ [HTTP2_HEADER_PATH]: '/' });
      req.on('error', (err) => {
        error = err;
      });
      req.on('response', (_headers) => {
        req.on('data', (_chunk) => { });
        req.on('end', () => {
          clientSession.close();
          return done(error);
        });
      });
    });
  });
}
socketFinished(common.mustSucceed());
