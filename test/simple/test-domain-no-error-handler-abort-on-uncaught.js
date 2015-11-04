'use strict';

/*
 * This test makes sure that when using --abort-on-uncaught-exception and
 * when throwing an error from within a domain that does not have an error
 * handler setup, the process aborts.
 */
var common = require('../common');
var assert = require('assert');
var domain = require('domain');
var child_process = require('child_process');

var tests = [
  function() {
    var d = domain.create();

    d.run(function() {
      throw new Error('boom!');
    });
  },

  function() {
    var d = domain.create();
    var d2 = domain.create();

    d.run(function() {
      d2.run(function() {
        throw new Error('boom!');
      });
    });
  },

  function() {
    var d = domain.create();

    d.run(function() {
      setTimeout(function() {
        throw new Error('boom!');
      });
    });
  },

  function() {
    var d = domain.create();

    d.run(function() {
      setImmediate(function() {
        throw new Error('boom!');
      });
    });
  },

  function() {
    var d = domain.create();

    d.run(function() {
      process.nextTick(function() {
        throw new Error('boom!');
      });
    });
  },

  function() {
    var d = domain.create();

    d.run(function() {
      var fs = require('fs');
      fs.exists('/non/existing/file', function onExists(exists) {
        throw new Error('boom!');
      });
    });
  },

  function() {
    var d = domain.create();
    var d2 = domain.create();

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
    var d = domain.create();
    var d2 = domain.create();

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
    var d = domain.create();
    var d2 = domain.create();

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
    var d = domain.create();
    var d2 = domain.create();

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
  var testIndex = +process.argv[3];
  tests[testIndex]();
} else {

  tests.forEach(function(test, testIndex) {
    var testCmd = '';
    if (process.platform !== 'win32') {
      // Do not create core files, as it can take a lot of disk space on
      // continuous testing and developers' machines
      testCmd += 'ulimit -c 0 && ';
    }

    testCmd +=  process.argv[0];
    testCmd += ' ' + '--abort-on-uncaught-exception';
    testCmd += ' ' + process.argv[1];
    testCmd += ' ' + 'child';
    testCmd += ' ' + testIndex;

    var child = child_process.exec(testCmd);

    child.on('exit', function onExit(code, signal) {
      assert.ok([132, 133, 134].indexOf(code) !== -1, 'Test at index ' +
        testIndex + ' should have aborted but instead exited with code ' +
        code + ' and signal ' + signal);
    });
  });
}
