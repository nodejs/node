'use strict';

// A minimal fixture that issues a single HTTP GET request using a
// user-provided agent. It is used to exercise how an agent picks up the
// environment proxy configuration when Node.js is started with
// --use-env-proxy or NODE_USE_ENV_PROXY.
//
// When PROXY_ENV_NULL is set, the agent is created with an explicit
// `proxyEnv: null`, which should disable proxying even when Node.js is
// configured to use the environment proxy. Otherwise a plain agent is
// created without any `proxyEnv` option, which should fall back to
// process.env when the environment proxy is enabled.

const http = require('http');

const url = process.env.REQUEST_URL;

const agent = process.env.PROXY_ENV_NULL ?
  new http.Agent({ proxyEnv: null }) :
  new http.Agent();

const req = http.get(url, { agent }, (res) => {
  res.pipe(process.stdout);
});

req.on('error', (e) => {
  console.error('Request Error', e);
});
