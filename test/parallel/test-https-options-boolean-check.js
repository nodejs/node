// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const fs = require('fs');

assert.doesNotThrow(() =>
  https.createServer({
    key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
    cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`)
  }));

assert.throws(() =>
  https.createServer({
    key: true,
    cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`)
  }), /"options\.key" must not be a boolean/);

assert.throws(() =>
  https.createServer({
    key: false,
    cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`)
  }), /"options\.key" must not be a boolean/);

assert.throws(() =>
  https.createServer({
    key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
    cert: true
  }), /"options\.cert" must not be a boolean/);

assert.throws(() =>
  https.createServer({
    key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
    cert: false
  }), /"options\.cert" must not be a boolean/);

assert.throws(() =>
  https.createServer({
    key: false,
    cert: false
  }), /"options\.key" must not be a boolean/);

assert.throws(() =>
  https.createServer({
    key: true,
    cert: true
  }), /"options\.key" must not be a boolean/);

assert.throws(() =>
  https.createServer({
    key: true,
    cert: false
  }), /"options\.key" must not be a boolean/);

assert.throws(() =>
  https.createServer({
    key: false,
    cert: true
  }), /"options\.key" must not be a boolean/);
