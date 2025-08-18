const assert = require('assert');
const http = require('http');
const { debuglog } = require('util');

// Set NODE_DEBUG for the test
process.env.NODE_DEBUG = 'http';

const server = http.createServer((req, res) => res.end());
server.timeout = 1;  // Force quick timeout

server.listen(0, () => {
  const req = http.request(`http://localhost:${server.address().port}`);
  // Add assertions to capture and verify debug output (e.g., via console spy)
});
