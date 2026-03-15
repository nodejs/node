'use strict';

const tls = require('tls');
const { parentPort } = require('worker_threads');

parentPort.postMessage({
  bundledLen: tls.getCACertificates('bundled').length,
  systemLen: tls.getCACertificates('system').length,
  defaultLen: tls.getCACertificates('default').length,
});
