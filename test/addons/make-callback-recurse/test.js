'use strict';

const common = require('../../common');
const assert = require('assert');
const domain = require('domain');
const binding = require(`./build/${common.buildType}/binding`);
const makeCallback = binding.makeCallback;

// Make sure this is run in the future.
const mustCallCheckDomains = common.mustCall(checkDomains);

// Make sure that using MakeCallback allows the error to propagate.
assert.throws(() => {
  makeCallback({}, () => {
    throw new Error('hi from domain error');
  });
}, /^Error: hi from domain error$/);


// Check the execution order of the nextTickQueue and MicrotaskQueue in
// relation to running multiple MakeCallback's from bootstrap,
// node::MakeCallback() and node::AsyncWrap::MakeCallback().
// TODO(trevnorris): Is there a way to verify this is being run during
// bootstrap?
(function verifyExecutionOrder(arg) {
  const results = [];

  // Processing of the MicrotaskQueue is manually handled by node. They are not
  // processed until after the nextTickQueue has been processed.
  Promise.resolve(1).then(common.mustCall(() => {
    results.push(7);
  }));

  // The nextTick should run after all immediately invoked calls.
  process.nextTick(common.mustCall(() => {
    results.push(3);

    // Run same test again but while processing the nextTickQueue to make sure
    // the following MakeCallback call breaks in the middle of processing the
    // queue and allows the script to run normally.
    process.nextTick(common.mustCall(() => {
      results.push(6);
    }));

    makeCallback({}, common.mustCall(() => {
      results.push(4);
    }));

    results.push(5);
  }));

  results.push(0);

  // MakeCallback is calling the function immediately, but should then detect
  // that a script is already in the middle of execution and return before
  // either the nextTickQueue or MicrotaskQueue are processed.
  makeCallback({}, common.mustCall(() => {
    results.push(1);
  }));

  // This should run before either the nextTickQueue or MicrotaskQueue are
  // processed. Previously MakeCallback would not detect this circumstance
  // and process them immediately.
  results.push(2);

  setImmediate(common.mustCall(() => {
    for (let i = 0; i < results.length; i++) {
      assert.strictEqual(results[i], i,
                         `verifyExecutionOrder(${arg}) results: ${results}`);
    }
    if (arg === 1) {
      // The tests are first run on bootstrap during LoadEnvironment() in
      // src/node.cc. Now run the tests through node::MakeCallback().
      setImmediate(() => {
        makeCallback({}, common.mustCall(() => {
          verifyExecutionOrder(2);
        }));
      });
    } else if (arg === 2) {
      // Make sure there are no conflicts using node::MakeCallback()
      // within timers.
      setTimeout(common.mustCall(() => {
        verifyExecutionOrder(3);
      }), 10);
    } else if (arg === 3) {
      mustCallCheckDomains();
    } else {
      throw new Error('UNREACHABLE');
    }
  }));
}(1));


function checkDomains() {
  // Check that domains are properly entered/exited when called in multiple
  // levels from both node::MakeCallback() and AsyncWrap::MakeCallback
  setImmediate(common.mustCall(() => {
    const d1 = domain.create();
    const d2 = domain.create();
    const d3 = domain.create();

    makeCallback({ domain: d1 }, common.mustCall(() => {
      assert.strictEqual(d1, process.domain);
      makeCallback({ domain: d2 }, common.mustCall(() => {
        assert.strictEqual(d2, process.domain);
        makeCallback({ domain: d3 }, common.mustCall(() => {
          assert.strictEqual(d3, process.domain);
        }));
        assert.strictEqual(d2, process.domain);
      }));
      assert.strictEqual(d1, process.domain);
    }));
  }));

  setTimeout(common.mustCall(() => {
    const d1 = domain.create();
    const d2 = domain.create();
    const d3 = domain.create();

    makeCallback({ domain: d1 }, common.mustCall(() => {
      assert.strictEqual(d1, process.domain);
      makeCallback({ domain: d2 }, common.mustCall(() => {
        assert.strictEqual(d2, process.domain);
        makeCallback({ domain: d3 }, common.mustCall(() => {
          assert.strictEqual(d3, process.domain);
        }));
        assert.strictEqual(d2, process.domain);
      }));
      assert.strictEqual(d1, process.domain);
    }));
  }), 1);

  function testTimer(id) {
    // Make sure nextTick, setImmediate and setTimeout can all recover properly
    // after a thrown makeCallback call.
    const d = domain.create();
    d.on('error', common.mustCall((e) => {
      assert.strictEqual(e.message, `throw from domain ${id}`);
    }));
    makeCallback({ domain: d }, () => {
      throw new Error(`throw from domain ${id}`);
    });
    throw new Error('UNREACHABLE');
  }

  process.nextTick(common.mustCall(testTimer), 3);
  setImmediate(common.mustCall(testTimer), 2);
  setTimeout(common.mustCall(testTimer), 1, 1);
}
