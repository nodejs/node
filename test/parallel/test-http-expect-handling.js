// Spec documentation http://httpwg.github.io/specs/rfc7231.html#header.expect
'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const tests = [417, 417];

let testsComplete = 0;
let testIdx = 0;

const s = http.createServer(function(req, res) {
  throw new Error('this should never be executed');
});

s.listen(0, nextTest);

function nextTest() {
  const options = {
    port: s.address().port,
    headers: { 'Expect': 'meoww' }
  };

  if (testIdx === tests.length) {
    return s.close();
  }

  const test = tests[testIdx];

  if (testIdx > 0) {
    s.on('checkExpectation', common.mustCall((req, res) => {
      res.statusCode = 417;
      res.end();
    }));
  }

  http.get(options, function(response) {
    console.log(`client: expected status: ${test}`);
    console.log(`client: statusCode: ${response.statusCode}`);
    assert.strictEqual(response.statusCode, test);
    assert.strictEqual(response.statusMessage, 'Expectation Failed');

    response.on('end', function() {
      testsComplete++;
      testIdx++;
      nextTest();
    });
    response.resume();
  });
}


process.on('exit', function() {
  assert.strictEqual(2, testsComplete);
});
