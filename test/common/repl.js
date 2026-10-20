'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const repl = require('node:repl');

common.skipIfInspectorDisabled();

function startNewREPLServer(replOpts = {}) {
  const input = new ArrayStream();
  const output = new ArrayStream();

  output.accumulator = '';
  output.write = (data) => (output.accumulator += `${data}`.replaceAll('\r', ''));

  const replServer = repl.start({
    prompt: '',
    input,
    output,
    terminal: true,
    allowBlockingCompletions: true,
    ...replOpts,
  });

  // The REPL evaluates input asynchronously, so a line written to `input` is
  // not necessarily reflected in `output` by the time control returns. Track
  // outstanding evaluations; `waitForIdle()` resolves once the count is zero
  // AND has been zero across a macrotask boundary (so synchronously-pumped
  // follow-up lines have a chance to register).
  let inFlight = 0;
  let settledTurns = 0;
  const originalEval = replServer.eval;
  replServer.eval = function(code, context, file, cb) {
    inFlight++;
    let decremented = false;
    const settle = () => {
      if (!decremented) {
        decremented = true;
        inFlight--;
        settledTurns = 0; // New completion resets the idle streak
      }
    };
    return originalEval.call(this, code, context, file, function(...args) {
      settle();
      return cb.apply(this, args);
    });
  };

  function waitForIdle() {
    return new Promise((resolve) => {
      function check() {
        if (inFlight === 0) {
          // Require two consecutive idle turns so any line the REPL pumped
          // synchronously after a completion has registered as in-flight.
          if (++settledTurns >= 2) {
            resolve();
            return;
          }
        } else {
          settledTurns = 0;
        }
        setImmediate(check);
      }
      // Start on a macrotask boundary so eval kicked off by `run()` registers.
      settledTurns = 0;
      setImmediate(check);
    });
  }

  // Feed `data` into the REPL and resolve once evaluation has settled. `data`
  // may be a single string (written verbatim) or an array of lines (each is
  // written followed by a newline, mirroring `ArrayStream.prototype.run`).
  function run(data) {
    if (Array.isArray(data)) {
      input.run(data);
    } else {
      input.emit('data', data);
    }
    return waitForIdle();
  }

  return { replServer, input, output, waitForIdle, run };
}


function complete(replServer, line) {
  return new Promise((resolve, reject) => {
    replServer.complete(line, (err, data) => {
      if (err) {
        reject(err);
      } else {
        resolve(data);
      }
    });
  });
}

module.exports = { startNewREPLServer, complete };
