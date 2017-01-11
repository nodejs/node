// Called by test/pummel/test-regress-GH-892.js

const https = require('https');
const fs = require('fs');
const assert = require('assert');

const PORT = parseInt(process.argv[2]);
const bytesExpected = parseInt(process.argv[3]);

let gotResponse = false;

const options = {
  method: 'POST',
  port: PORT,
  rejectUnauthorized: false
};

const req = https.request(options, (res) => {
  assert.strictEqual(200, res.statusCode);
  gotResponse = true;
  console.error('DONE');
  res.resume();
});

req.end(Buffer.allocUnsafe(bytesExpected));

process.on('exit', () => {
  assert.ok(gotResponse);
});
