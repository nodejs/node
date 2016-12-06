'use strict';

// This test makes sure that when using --abort-on-uncaught-exception and
// when throwing an error from within a domain that has an error handler
// setup, the process _does not_ abort.

const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const child_process = require('child_process');

var errorHandlerCalled = false;

const tests = [
  function nextTick() {
    const d = domain.create();

    d.once('error', function(err) {
      errorHandlerCalled = true;
    });

    d.run(function() {
      process.nextTick(function() {
        throw new Error('exceptional!');
      });
    });
  },

  function timer() {
    const d = domain.create();

    d.on('error', function(err) {
      errorHandlerCalled = true;
    });

    d.run(function() {
      setTimeout(function() {
        throw new Error('exceptional!');
      }, 33);
    });
  },

  function immediate() {
    const d = domain.create();

    d.on('error', function errorHandler() {
      errorHandlerCalled = true;
    });

    d.run(function() {
      setImmediate(function() {
        throw new Error('boom!');
      });
    });
  },

  function timerPlusNextTick() {
    const d = domain.create();

    d.on('error', function(err) {
      errorHandlerCalled = true;
    });

    d.run(function() {
      setTimeout(function() {
        process.nextTick(function() {
          throw new Error('exceptional!');
        });
      }, 33);
    });
  },

  function firstRun() {
    const d = domain.create();

    d.on('error', function(err) {
      errorHandlerCalled = true;
    });

    d.run(function() {
      throw new Error('exceptional!');
    });
  },

  function fsAsync() {
    const d = domain.create();

    d.on('error', function errorHandler() {
      errorHandlerCalled = true;
    });

    d.run(function() {
      var fs = require('fs');
      fs.exists('/non/existing/file', function onExists(exists) {
        throw new Error('boom!');
      });
    });
  },

  function netServer() {
    const net = require('net');
    const d = domain.create();

    d.on('error', function(err) {
      errorHandlerCalled = true;
    });

    d.run(function() {
      const server = net.createServer(function(conn) {
        conn.pipe(conn);
      });
      server.listen(0, common.localhostIPv4, function() {
        const conn = net.connect(this.address().port, common.localhostIPv4);
        conn.once('data', function() {
          throw new Error('ok');
        });
        conn.end('ok');
        server.close();
      });
    });
  },

  function firstRunOnlyTopLevelErrorHandler() {
    const d = domain.create();
    const d2 = domain.create();

    d.on('error', function errorHandler() {
      errorHandlerCalled = true;
    });

    d.run(function() {
      d2.run(function() {
        throw new Error('boom!');
      });
    });
  },

  function firstRunNestedWithErrorHandler() {
    const d = domain.create();
    const d2 = domain.create();

    d2.on('error', function errorHandler() {
      errorHandlerCalled = true;
    });

    d.run(function() {
      d2.run(function() {
        throw new Error('boom!');
      });
    });
  },

  function timeoutNestedWithErrorHandler() {
    const d = domain.create();
    const d2 = domain.create();

    d2.on('error', function errorHandler() {
      errorHandlerCalled = true;
    });

    d.run(function() {
      d2.run(function() {
        setTimeout(function() {
          console.log('foo');
          throw new Error('boom!');
        }, 33);
      });
    });
  },

  function setImmediateNestedWithErrorHandler() {
    const d = domain.create();
    const d2 = domain.create();

    d2.on('error', function errorHandler() {
      errorHandlerCalled = true;
    });

    d.run(function() {
      d2.run(function() {
        setImmediate(function() {
          throw new Error('boom!');
        });
      });
    });
  },

  function nextTickNestedWithErrorHandler() {
    const d = domain.create();
    const d2 = domain.create();

    d2.on('error', function errorHandler() {
      errorHandlerCalled = true;
    });

    d.run(function() {
      d2.run(function() {
        process.nextTick(function() {
          throw new Error('boom!');
        });
      });
    });
  },

  function fsAsyncNestedWithErrorHandler() {
    const d = domain.create();
    const d2 = domain.create();

    d2.on('error', function errorHandler() {
      errorHandlerCalled = true;
    });

    d.run(function() {
      d2.run(function() {
        var fs = require('fs');
        fs.exists('/non/existing/file', function onExists(exists) {
          throw new Error('boom!');
        });
      });
    });
  }
];

if (process.argv[2] === 'child') {
  const testIndex = +process.argv[3];

  tests[testIndex]();

  process.on('exit', function onExit() {
    assert.strictEqual(errorHandlerCalled, true);
  });
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

    child.on('exit', function onExit(code, signal) {
      assert.strictEqual(code, 0, 'Test at index ' + testIndex +
        ' should have exited with exit code 0 but instead exited with code ' +
        code + ' and signal ' + signal);
    });
  });
}
