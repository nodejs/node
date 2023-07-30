// Flags: --test --expose-internals
'use strict';

const v8_reporter = require('internal/test_runner/reporter/v8-serializer');
const { Buffer } = require('buffer');


(async function () {
  const reported = v8_reporter([
    { type: "test:pass", data: { name: "test", nesting: 0, file: __filename, testNumber: 1, details: { duration_ms: 0 } } }
  ]);

  for await (const chunk of reported) {
    process.stdout.write(chunk);
    process.stdout.write(Buffer.concat([Buffer.from("arbitrary - pre"), chunk]));
    process.stdout.write(Buffer.from("arbitrary - mid"));
    process.stdout.write(Buffer.concat([chunk, Buffer.from("arbitrary - post")]));
  }
})();

