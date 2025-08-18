const assert = require('assert');
const http = require('http');
const server = http.createServer((req, res) => res.end());
server.timeout = 1;  // Force quick timeout
server.listen(0, () => {
  const req = http.request(`http://localhost:${server.address().port}`);
  // Simulate timeout and check logs (use console spy or similar)
});
