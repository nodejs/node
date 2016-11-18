'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

assert.strictEqual('NODE_UNIQUE_ID' in process.env, false,
                   'NODE_UNIQUE_ID should be removed on startup');

function forEach(obj, fn) {
  Object.keys(obj).forEach((name, index) => {
    fn(obj[name], name, index);
  });
}


if (cluster.isWorker) {
  const http = require('http');
  http.Server(function() {

  }).listen(common.PORT, '127.0.0.1');
} else if (cluster.isMaster) {

  const checks = {
    cluster: {
      events: {
        fork: false,
        online: false,
        listening: false,
        exit: false
      },
      equal: {
        fork: false,
        online: false,
        listening: false,
        exit: false
      }
    },

    worker: {
      events: {
        online: false,
        listening: false,
        exit: false
      },
      equal: {
        online: false,
        listening: false,
        exit: false
      },
      states: {
        none: false,
        online: false,
        listening: false,
        dead: false
      }
    }
  };

  var worker;
  const stateNames = Object.keys(checks.worker.states);

  //Check events, states, and emit arguments
  forEach(checks.cluster.events, (bool, name, index) => {

    //Listen on event
    cluster.on(name, common.mustCall(function(/* worker */) {

      //Set event
      checks.cluster.events[name] = true;

      //Check argument
      checks.cluster.equal[name] = worker === arguments[0];

      //Check state
      var state = stateNames[index];
      checks.worker.states[state] = (state === worker.state);
    }));
  });

  //Kill worker when listening
  cluster.on('listening', common.mustCall(() => {
    worker.kill();
  }));

  //Kill process when worker is killed
  cluster.on('exit', common.mustCall(() => {}));

  //Create worker
  worker = cluster.fork();
  assert.strictEqual(worker.id, 1);
  assert(worker instanceof cluster.Worker,
         'the worker is not a instance of the Worker constructor');

  //Check event
  forEach(checks.worker.events, function(bool, name, index) {
    worker.on(name, common.mustCall(function() {
      //Set event
      checks.worker.events[name] = true;

      //Check argument
      checks.worker.equal[name] = (worker === this);

      switch (name) {
        case 'exit':
          assert.strictEqual(arguments[0], worker.process.exitCode);
          assert.strictEqual(arguments[1], worker.process.signalCode);
          assert.strictEqual(arguments.length, 2);
          break;

        case 'listening':
          assert.strictEqual(arguments.length, 1);
          const expect = { address: '127.0.0.1',
                           port: common.PORT,
                           addressType: 4,
                           fd: undefined };
          assert.deepStrictEqual(arguments[0], expect);
          break;

        default:
          assert.strictEqual(arguments.length, 0);
          break;
      }
    }));
  });

  //Check all values
  process.once('exit', () => {
    //Check cluster events
    forEach(checks.cluster.events, (check, name) => {
      assert(check,
             `The cluster event "${name}" on the cluster object did not fire`);
    });

    //Check cluster event arguments
    forEach(checks.cluster.equal, (check, name) => {
      assert(check,
             `The cluster event "${name}" did not emit with correct argument`);
    });

    //Check worker states
    forEach(checks.worker.states, (check, name) => {
      assert(check,
             `The worker state "${name}" was not set to true`);
    });

    //Check worker events
    forEach(checks.worker.events, (check, name) => {
      assert(check,
             `The worker event "${name}" on the worker object did not fire`);
    });

    //Check worker event arguments
    forEach(checks.worker.equal, (check, name) => {
      assert(check,
             `The worker event "${name}" did not emit with correct argument`);
    });
  });

}
