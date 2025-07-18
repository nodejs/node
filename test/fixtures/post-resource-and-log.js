const url = process.env.REQUEST_URL;
const resourceData = process.env.RESOURCE_DATA || '{"name":"test","value":"data"}';

let lib;
if (url.startsWith('https')) {
  lib = require('https');
} else {
  lib = require('http');
}

let timeout;
if (process.env.REQUEST_TIMEOUT) {
  timeout = parseInt(process.env.REQUEST_TIMEOUT, 10);
}

let agent;
if (process.env.AGENT_TIMEOUT) {
  agent = new lib.Agent({
    proxyEnv: process.env,
    timeout: parseInt(process.env.AGENT_TIMEOUT, 10)
  });
}

const options = {
  method: 'POST',
  timeout,
  agent,
  headers: {
    'Content-Type': 'application/json',
    'Content-Length': Buffer.byteLength(resourceData)
  }
};

const req = lib.request(url, options, (res) => {
  console.log(`Status Code: ${res.statusCode}`);
  console.log('Headers:', res.headers);
  res.pipe(process.stdout);
});

req.on('error', (e) => {
  console.error('Request Error', e);
});

req.on('timeout', () => {
  console.error('Request timed out');
  req.destroy();
});

req.write(resourceData);
req.end();
