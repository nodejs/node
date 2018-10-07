'use strict';

const rulesDirs = ['tools/eslint-rules'];
const extensions = ['.js', '.md'];
// This is the maximum number of files to be linted per worker at any given time
const maxWorkload = 40;

const cluster = require('cluster');
const path = require('path');
const fs = require('fs');
const totalCPUs = require('os').cpus().length;

const CLIEngine = require('eslint').CLIEngine;
const glob = require('eslint/node_modules/glob');

const cliOptions = {
  rulePaths: rulesDirs,
  extensions: extensions,
};

// Check if we should fix errors that are fixable
if (process.argv.indexOf('-F') !== -1)
  cliOptions.fix = true;

const cli = new CLIEngine(cliOptions);

if (cluster.isMaster) {
  var numCPUs = 1;
  const paths = [];
  var files = null;
  var totalPaths = 0;
  var failures = 0;
  var successes = 0;
  var lastLineLen = 0;
  var curPath = 'Starting ...';
  var showProgress = true;
  const globOptions = {
    nodir: true
  };
  const workerConfig = {};
  var startTime;
  var formatter;
  var outFn;
  var fd;
  var i;

  // Check if spreading work among all cores/cpus
  if (process.argv.indexOf('-J') !== -1)
    numCPUs = totalCPUs;

  // Check if spreading work among an explicit number of cores/cpus
  i = process.argv.indexOf('-j');
  if (i !== -1) {
    if (!process.argv[i + 1])
      throw new Error('Missing parallel job count');
    numCPUs = parseInt(process.argv[i + 1], 10);
    if (!isFinite(numCPUs) || numCPUs <= 0)
      throw new Error('Bad parallel job count');
  }

  // Check for custom ESLint report formatter
  i = process.argv.indexOf('-f');
  if (i !== -1) {
    if (!process.argv[i + 1])
      throw new Error('Missing format name');
    const format = process.argv[i + 1];
    formatter = cli.getFormatter(format);
    if (!formatter)
      throw new Error('Invalid format name');
    // Automatically disable progress display
    showProgress = false;
    // Tell worker to send all results, not just linter errors
    workerConfig.sendAll = true;
  } else {
    // Use default formatter
    formatter = cli.getFormatter();
  }

  // Check if outputting ESLint report to a file instead of stdout
  i = process.argv.indexOf('-o');
  if (i !== -1) {
    if (!process.argv[i + 1])
      throw new Error('Missing output filename');
    const outPath = path.resolve(process.argv[i + 1]);
    fd = fs.openSync(outPath, 'w');
    outFn = function(str) {
      fs.writeSync(fd, str, 'utf8');
    };
    process.on('exit', function() {
      fs.closeSync(fd);
    });
  } else {
    outFn = function(str) {
      process.stdout.write(str);
    };
  }

  // Process the rest of the arguments as paths to lint, ignoring any unknown
  // flags
  for (i = 2; i < process.argv.length; ++i) {
    if (process.argv[i][0] === '-') {
      switch (process.argv[i]) {
        case '-f': // Skip format name
        case '-o': // Skip filename
        case '-j': // Skip parallel job count number
          ++i;
          break;
      }
      continue;
    }
    paths.push(process.argv[i]);
  }

  if (paths.length === 0)
    return;
  totalPaths = paths.length;

  if (showProgress) {
    // Start the progress display update timer when the first worker is ready
    cluster.once('online', function() {
      startTime = process.hrtime();
      setInterval(printProgress, 1000).unref();
      printProgress();
    });
  }

  cluster.on('online', function(worker) {
    // Configure worker and give it some initial work to do
    worker.send(workerConfig);
    sendWork(worker);
  });

  process.on('exit', function(code) {
    if (showProgress) {
      curPath = 'Done';
      printProgress();
      outFn('\r\n');
    }
    if (code === 0)
      process.exit(failures ? 1 : 0);
  });

  for (i = 0; i < numCPUs; ++i)
    cluster.fork().on('message', onWorkerMessage).on('exit', onWorkerExit);

  function onWorkerMessage(results) {
    if (typeof results !== 'number') {
      // The worker sent us results that are not all successes
      if (workerConfig.sendAll) {
        failures += results.errorCount;
        results = results.results;
      } else {
        failures += results.length;
      }
      outFn(`${formatter(results)}\r\n`);
      printProgress();
    } else {
      successes += results;
    }
    // Try to give the worker more work to do
    sendWork(this);
  }

  function onWorkerExit(code, signal) {
    if (code !== 0 || signal)
      process.exit(2);
  }

  function sendWork(worker) {
    if (!files || !files.length) {
      // We either just started or we have no more files to lint for the current
      // path. Find the next path that has some files to be linted.
      while (paths.length) {
        var dir = paths.shift();
        curPath = dir;
        const patterns = cli.resolveFileGlobPatterns([dir]);
        dir = path.resolve(patterns[0]);
        files = glob.sync(dir, globOptions);
        if (files.length)
          break;
      }
      if ((!files || !files.length) && !paths.length) {
        // We exhausted all input paths and thus have nothing left to do, so end
        // the worker
        return worker.disconnect();
      }
    }
    // Give the worker an equal portion of the work left for the current path,
    // but not exceeding a maximum file count in order to help keep *all*
    // workers busy most of the time instead of only a minority doing most of
    // the work.
    const sliceLen = Math.min(maxWorkload, Math.ceil(files.length / numCPUs));
    var slice;
    if (sliceLen === files.length) {
      // Micro-optimization to avoid splicing to an empty array
      slice = files;
      files = null;
    } else {
      slice = files.splice(0, sliceLen);
    }
    worker.send(slice);
  }

  function printProgress() {
    if (!showProgress)
      return;

    // Clear line
    outFn(`\r ${' '.repeat(lastLineLen)}\r`);

    // Calculate and format the data for displaying
    const elapsed = process.hrtime(startTime)[0];
    const mins = `${Math.floor(elapsed / 60)}`.padStart(2, '0');
    const secs = `${elapsed % 60}`.padStart(2, '0');
    const passed = `${successes}`.padStart(6);
    const failed = `${failures}`.padStart(6);
    var pct = Math.ceil(((totalPaths - paths.length) / totalPaths) * 100);
    pct = `${pct}`.padStart(3);

    var line = `[${mins}:${secs}|%${pct}|+${passed}|-${failed}]: ${curPath}`;

    // Truncate line like cpplint does in case it gets too long
    if (line.length > 75)
      line = `${line.slice(0, 75)}...`;

    // Store the line length so we know how much to erase the next time around
    lastLineLen = line.length;

    outFn(line);
  }
} else {
  // Worker

  var config = {};
  process.on('message', function(files) {
    if (files instanceof Array) {
      // Lint some files
      const report = cli.executeOnFiles(files);

      // If we were asked to fix the fixable issues, do so.
      if (cliOptions.fix)
        CLIEngine.outputFixes(report);

      if (config.sendAll) {
        // Return both success and error results

        const results = report.results;
        // Silence warnings for files with no errors while keeping the "ok"
        // status
        if (report.warningCount > 0) {
          for (var i = 0; i < results.length; ++i) {
            const result = results[i];
            if (result.errorCount === 0 && result.warningCount > 0) {
              result.warningCount = 0;
              result.messages = [];
            }
          }
        }
        process.send({ results: results, errorCount: report.errorCount });
      } else if (report.errorCount === 0) {
        // No errors, return number of successful lint operations
        process.send(files.length);
      } else {
        // One or more errors, return the error results only
        process.send(CLIEngine.getErrorResults(report.results));
      }
    } else if (typeof files === 'object') {
      // The master process is actually sending us our configuration and not a
      // list of files to lint
      config = files;
    }
  });
}
