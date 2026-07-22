'use strict';

const common = require('../common');

const assert = require('assert');
const http = require('http');

const durationBetweenIntervals = [];
let timeoutTooShort = false;
const TIMEOUT = common.platformTimeout(200);
const INTERVAL = Math.floor(TIMEOUT / 8);

runTest(TIMEOUT);

function runTest(timeoutDuration) {
  let intervalWasInvoked = false;
  let newTimeoutDuration = 0;
  const closeCallback = (err) => {
    assert.ifError(err);
    if (newTimeoutDuration) {
      runTest(newTimeoutDuration);
    }
  };

  const server = http.createServer((req, res) => {
    server.close(common.mustCall(closeCallback));

    res.writeHead(200);
    res.flushHeaders();

    req.setTimeout(timeoutDuration, () => {
      if (!intervalWasInvoked) {
        // Interval wasn't invoked, probably because the machine is busy with
        // other things. Try again with a longer timeout.
        newTimeoutDuration = timeoutDuration * 2;
        console.error('The interval was not invoked.');
        console.error(`Trying w/ timeout of ${newTimeoutDuration}.`);
        return;
      }

      if (timeoutTooShort) {
        intervalWasInvoked = false;
        timeoutTooShort = false;
        newTimeoutDuration =
          Math.max(...durationBetweenIntervals, timeoutDuration) * 2;
        console.error(`Time between intervals: ${durationBetweenIntervals}`);
        console.error(`Trying w/ timeout of ${newTimeoutDuration}`);
        return;
      }

      assert.fail('Request timeout should not fire');
    });

    req.resume();
    req.once('end', () => {
      res.end();
    });
  });

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port,
      method: 'POST'
    }, () => {
      let lastIntervalTimestamp = Date.now();
      const interval = setInterval(() => {
        const lastDuration = Date.now() - lastIntervalTimestamp;
        durationBetweenIntervals.push(lastDuration);
        lastIntervalTimestamp = Date.now();
        if (lastDuration > timeoutDuration / 2) {
          // The interval is supposed to be about 1/8 of the timeout duration.
          // If it's running so infrequently that it's greater than 1/2 the
          // timeout duration, then run the test again with a longer timeout.
          timeoutTooShort = true;
        }
        intervalWasInvoked = true;
        req.write('a');
      }, INTERVAL);
      setTimeout(() => {
        clearInterval(interval);
        req.end();
      }, timeoutDuration);
    });
    req.write('.');
  }));
}
