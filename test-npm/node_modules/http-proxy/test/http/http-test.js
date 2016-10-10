/*
  node-http-proxy-test.js: http proxy for node.js

  Copyright (c) 2010 Charlie Robbins, Marak Squires and Fedor Indutny

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/
 
var assert = require('assert'),
    fs = require('fs'),
    path = require('path'),
    async = require('async'),
    request = require('request'),
    vows = require('vows'),
    macros = require('../macros'),
    helpers = require('../helpers');

var routeFile = path.join(__dirname, 'config.json');

vows.describe(helpers.describe()).addBatch({
  "With a valid target server": {
    "and no latency": {
      "and no headers": macros.http.assertProxied(),
      "and headers": macros.http.assertProxied({
        request: { headers: { host: 'unknown.com' } }
      }),
      "and forwarding enabled": macros.http.assertForwardProxied()
    },
    "and latency": macros.http.assertProxied({
      latency: 2000
    })
  },
  "With a no valid target server": {
    "and no latency": macros.http.assertInvalidProxy(),
    "and latency": macros.http.assertInvalidProxy({
      latency: 2000
    })
  }
}).export(module);