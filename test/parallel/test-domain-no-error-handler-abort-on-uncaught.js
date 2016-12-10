'use strict';

/*
 * This test makes sure that when using --abort-on-uncaught-exception and
 * when throwing an error from within a domain that does not have an error
 * handler setup, the process aborts.
 */
const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const child_process = require('child_process');

const tests = [
  function() {
    const d = domain.create();

    d.run(function() {
      throw new Error('boom!');
    });
  },

  function() {
    const d = domain.create();
    const d2 = domain.create();

    d.run(function() {
      d2.run(function() {
        throw new Error('boom!');
      });
    });
  },

  function() {
    const d = domain.create();

    d.run(function() {
      setTimeout(function() {
        throw new Error('boom!');
      });
    });
  },

  function() {
    const d = domain.create();

    d.run(function() {
      setImmediate(function() {
        throw new Error('boom!');
      });
    });
  },

  function() {
    const d = domain.create();

    d.run(function() {
      process.nextTick(function() {
        throw new Error('boom!');
      });
    });
  },

  function() {
    const d = domain.create();

    d.run(function() {
      var fs = require('fs');
      fs.exists('/non/existing/file', function onExists(exists) {
        throw new Error('boom!');
      });
    });
  },

  function() {
    const d = domain.create();
    const d2 = domain.create();

    d.on('error', function errorHandler() {
    });

    d.run(function() {
      d2.run(function() {
        setTimeout(function() {
          throw new Error('boom!');
        });
      });
    });
  },

  function() {
    const d = domain.create();
    const d2 = domain.create();

    d.on('error', function errorHandler() {
    });

    d.run(function() {
      d2.run(function() {
        setImmediate(function() {
          throw new Error('boom!');
        });
      });
    });
  },

  function() {
    const d = domain.create();
    const d2 = domain.create();

    d.on('error', function errorHandler() {
    });

    d.run(function() {
      d2.run(function() {
        process.nextTick(function() {
          throw new Error('boom!');
        });
      });
    });
  },

  function() {
    const d = domain.create();
    const d2 = domain.create();

    d.on('error', function errorHandler() {
    });

    d.run(function() {
      d2.run(function() {
        var fs = require('fs');
        fs.exists('/non/existing/file', function onExists(exists) {
          throw new Error('boom!');
        });
      });
    });
  },
];

if (process.argv[2] === 'child') {
  const testIndex = +process.argv[3];
  tests[testIndex]();
} else {

  tests.forEach(function(test, testIndex) {
    var testCmd = '';
    if (!common.isWindows) {
      // Do not create core files, as it can take a lot of disk space on
      // continuous testing and developers' machines
      testCmd += 'ulimit -c 0 && ';
    }

    testCmd += process.argv[0];
    testCmd += ' ' + '--abort-on-uncaught-exception';
    testCmd += ' ' + process.argv[1];
    testCmd += ' ' + 'child';
    testCmd += ' ' + testIndex;

    var child = child_process.exec(testCmd);

    child.on('exit', function onExit(exitCode, signal) {
      const errMsg = 'Test at index ' + testIndex + ' should have aborted ' +
                     'but instead exited with exit code ' + exitCode +
                     ' and signal ' + signal;
      assert(common.nodeProcessAborted(exitCode, signal), errMsg);
    });
  });
}
