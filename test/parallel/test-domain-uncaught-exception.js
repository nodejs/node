'use strict';

/*
 * The goal of this test is to make sure that errors thrown within domains
 * are handled correctly. It checks that the process' 'uncaughtException' event
 * is emitted when appropriate, and not emitted when it shouldn't. It also
 * checks that the proper domain error handlers are called when they should
 * be called, and not called when they shouldn't.
 */

const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const child_process = require('child_process');

const tests = [];

function test1() {
  /*
   * Throwing from an async callback from within a domain that doesn't have
   * an error handler must result in emitting the process' uncaughtException
   * event.
   */
  const d = domain.create();
  d.run(function() {
    setTimeout(function onTimeout() {
      throw new Error('boom!');
    });
  });
}

tests.push({
  fn: test1,
  expectedMessages: ['uncaughtException']
});

function test2() {
  /*
   * Throwing from from within a domain that doesn't have an error handler must
   * result in emitting the process' uncaughtException event.
   */
  const d2 = domain.create();
  d2.run(function() {
    throw new Error('boom!');
  });
}

tests.push({
  fn: test2,
  expectedMessages: ['uncaughtException']
});

function test3() {
  /*
   * This test creates two nested domains: d3 and d4. d4 doesn't register an
   * error handler, but d3 does. The error is handled by the d3 domain and thus
   * an 'uncaughtException' event should _not_ be emitted.
   */
  const d3 = domain.create();
  const d4 = domain.create();

  d3.on('error', function onErrorInD3Domain(err) {
    process.send('errorHandledByDomain');
  });

  d3.run(function() {
    d4.run(function() {
      throw new Error('boom!');
    });
  });
}

tests.push({
  fn: test3,
  expectedMessages: ['errorHandledByDomain']
});

function test4() {
  /*
   * This test creates two nested domains: d5 and d6. d6 doesn't register an
   * error handler. When the timer's callback is called, because async
   * operations like timer callbacks are bound to the domain that was active
   * at the time of their creation, and because both d5 and d6 domains have
   * exited by the time the timer's callback is called, its callback runs with
   * only d6 on the domains stack. Since d6 doesn't register an error handler,
   * the process' uncaughtException event should be emitted.
   */
  const d5 = domain.create();
  const d6 = domain.create();

  d5.on('error', function onErrorInD2Domain(err) {
    process.send('errorHandledByDomain');
  });

  d5.run(function() {
    d6.run(function() {
      setTimeout(function onTimeout() {
        throw new Error('boom!');
      });
    });
  });
}

tests.push({
  fn: test4,
  expectedMessages: ['uncaughtException']
});

function test5() {
  /*
   * This test creates two nested domains: d7 and d8. d8 _does_ register an
   * error handler, so throwing within that domain should not emit an uncaught
   * exception.
   */
  const d7 = domain.create();
  const d8 = domain.create();

  d8.on('error', function onErrorInD3Domain(err) {
    process.send('errorHandledByDomain');
  });

  d7.run(function() {
    d8.run(function() {
      throw new Error('boom!');
    });
  });
}
tests.push({
  fn: test5,
  expectedMessages: ['errorHandledByDomain']
});

function test6() {
  /*
   * This test creates two nested domains: d9 and d10. d10 _does_ register an
   * error handler, so throwing within that domain in an async callback should
   * _not_ emit an uncaught exception.
   */
  const d9 = domain.create();
  const d10 = domain.create();

  d10.on('error', function onErrorInD2Domain(err) {
    process.send('errorHandledByDomain');
  });

  d9.run(function() {
    d10.run(function() {
      setTimeout(function onTimeout() {
        throw new Error('boom!');
      });
    });
  });
}

tests.push({
  fn: test6,
  expectedMessages: ['errorHandledByDomain']
});

if (process.argv[2] === 'child') {
  const testIndex = process.argv[3];
  process.on('uncaughtException', function onUncaughtException() {
    process.send('uncaughtException');
  });

  tests[testIndex].fn();
} else {
  // Run each test's function in a child process. Listen on
  // messages sent by each child process and compare expected
  // messages defined for each test with the actual received messages.
  tests.forEach(function doTest(test, testIndex) {
    const testProcess = child_process.fork(__filename, ['child', testIndex]);

    testProcess.on('message', function onMsg(msg) {
      if (test.messagesReceived === undefined)
        test.messagesReceived = [];

      test.messagesReceived.push(msg);
    });

    testProcess.on('disconnect', common.mustCall(function onExit() {
      // Make sure that all expected messages were sent from the
      // child process
      test.expectedMessages.forEach(function(expectedMessage) {
        if (test.messagesReceived === undefined ||
          test.messagesReceived.indexOf(expectedMessage) === -1)
          assert(false, 'test ' + test.fn.name +
              ' should have sent message: ' + expectedMessage +
              ' but didn\'t');
      });

      if (test.messagesReceived) {
        test.messagesReceived.forEach(function(receivedMessage) {
          if (test.expectedMessages.indexOf(receivedMessage) === -1) {
            assert(false, 'test ' + test.fn.name +
              ' should not have sent message: ' + receivedMessage +
              ' but did');
          }
        });
      }
    }));
  });
}
