// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var assert = require('assert');

var common = require('../common');

/*
 * The goal of this test is to make sure that:
 *
 * - Even if --abort_on_uncaught_exception is passed on the command line,
 * setting up a top-level domain error handler and throwing an error
 * within this domain does *not* make the process abort. The process exits
 * gracefully.
 *
 * - When passing --abort_on_uncaught_exception on the command line and
 * setting up a top-level domain error handler, an error thrown
 * within this domain's error handler *does* make the process abort.
 *
 * - When *not* passing --abort_on_uncaught_exception on the command line and
 * setting up a top-level domain error handler, an error thrown within this
 * domain's error handler does *not* make the process abort, but makes it exit
 * with the proper failure exit code.
 *
 * - When throwing an error within the top-level domain's error handler
 * within a try/catch block, the process should exit gracefully, whether or
 * not --abort_on_uncaught_exception is passed on the command line.
 */

var domainErrHandlerExMessage = 'exception from domain error handler';

if (process.argv[2] === 'child') {
  var domain = require('domain');
  var d = domain.create();
  var triggeredProcessUncaughtException = false;

  process.on('uncaughtException', function onUncaughtException() {
    // The process' uncaughtException event must not be emitted when
    // an error handler is setup on the top-level domain.
    // Exiting with exit code of 42 here so that it would assert when
    // the parent checks the child exit code.
    process.exit(42);
  });

  d.on('error', function(err) {
    // Swallowing the error on purpose if 'throwInDomainErrHandler' is not
    // set
    if (process.argv.indexOf('throwInDomainErrHandler') !== -1) {
      if (process.argv.indexOf('useTryCatch') !== -1) {
        try {
          throw new Error(domainErrHandlerExMessage);
        } catch (e) {
        }
      } else {
        throw new Error(domainErrHandlerExMessage);
      }
    }
  });

  d.run(function doStuff() {
    // Throwing from within different types of callbacks as each of them
    // handles domains differently
    process.nextTick(function () {
      throw new Error("Error from nextTick callback");
    });

    var fs = require('fs');
    fs.exists('/non/existing/file', function onExists(exists) {
      throw new Error("Error from fs.exists callback");
    });

    setImmediate(function onSetImmediate() {
      throw new Error("Error from setImmediate callback");
    });

    setTimeout(function() {
      throw new Error("Error from setTimeout's callback");
    }, 0);

    throw new Error("Error from domain.run callback");
  });
} else {
  var exec = require('child_process').exec;

  function testDomainExceptionHandling(cmdLineOption, options) {
    if (typeof cmdLineOption === 'object') {
      options = cmdLineOption;
      cmdLineOption = undefined;
    }

    var throwInDomainErrHandlerOpt;
    if (options.throwInDomainErrHandler)
      throwInDomainErrHandlerOpt = 'throwInDomainErrHandler';

    var cmdToExec = '';
    if (process.platform !== 'win32') {
      // Do not create core files, as it can take a lot of disk space on
      // continuous testing and developers' machines
      cmdToExec += 'ulimit -c 0 && ';
    }

    var useTryCatchOpt;
    if (options.useTryCatch)
      useTryCatchOpt = 'useTryCatch';

    cmdToExec +=  process.argv[0] + ' ';
    cmdToExec += (cmdLineOption ? cmdLineOption : '') + ' ';
    cmdToExec += process.argv[1] + ' ';
    cmdToExec += [
      'child',
      throwInDomainErrHandlerOpt,
      useTryCatchOpt
    ].join(' ');

    var child = exec(cmdToExec);

    if (child) {
      var childTriggeredOnUncaughtExceptionHandler = false;
      child.on('message', function onChildMsg(msg) {
        if (msg === 'triggeredProcessUncaughtEx') {
          childTriggeredOnUncaughtExceptionHandler = true;
        }
      });

      child.on('exit', function onChildExited(exitCode, signal) {
        // When throwing errors from the top-level domain error handler
        // outside of a try/catch block, the process should not exit gracefully
        if (!options.useTryCatch && options.throwInDomainErrHandler) {
          // If the top-level domain's error handler does not throw,
          // the process must exit gracefully, whether or not
          // --abort_on_uncaught_exception was passed on the command line
          if (cmdLineOption === '--abort_on_uncaught_exception') {
            assert(common.nodeProcessAborted(exitCode, signal),
                'process should have aborted, but did not');
          } else {
            // By default, uncaught exceptions make node exit with an exit
            // code of 7.
            assert.equal(exitCode, 7);
            assert.equal(signal, null);
          }
        } else {
          // If the top-level domain's error handler does not throw,
          // the process must exit gracefully, whether or not
          // --abort_on_uncaught_exception was passed on the command line
          assert.equal(exitCode, 0);
          assert.equal(signal, null);
        }
      });
    }
  }

  testDomainExceptionHandling('--abort_on_uncaught_exception', {
                              throwInDomainErrHandler: false,
                              useTryCatch: false
                            });

  testDomainExceptionHandling('--abort_on_uncaught_exception', {
                              throwInDomainErrHandler: false,
                              useTryCatch: true
                            });

  testDomainExceptionHandling('--abort_on_uncaught_exception', {
                              throwInDomainErrHandler: true,
                              useTryCatch: false
                            });

  testDomainExceptionHandling('--abort_on_uncaught_exception', {
                              throwInDomainErrHandler: true,
                              useTryCatch: true
                            });

  testDomainExceptionHandling({
    throwInDomainErrHandler: false
  });

  testDomainExceptionHandling({
    throwInDomainErrHandler: false,
    useTryCatch: false
  });

  testDomainExceptionHandling({
    throwInDomainErrHandler: true,
    useTryCatch: true
  });
}
