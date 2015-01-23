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

/*
 * This script updates the AUTHORS so that it contains a list of contributors'
 * names ordered alphabetically. The sorting algorithm used is stable, see
 * function "stableSort" in tools/common.js.
 */

'use strict';

var child_process = require('child_process');
var fs = require('fs');
var assert = require('assert');
var util = require('util');
var path = require('path');
var os = require('os');

var debug = util.debuglog('update-authors');

var commonTools = require(path.join(__dirname, './common.js'));

// Use '%aN <%aE>' and not '%an <%ae>' as we want to have git log use the
// .mailmap file.
var GIT_LOG_COMMAND_ARGS = ['log', "--format='%aN <%aE>'"];
var MAILMAP_FILE = '.mailmap';

var OPTS = processCmdLineArgs();
var AUTHORS_FILE = OPTS.authorsFile || 'AUTHORS';

var PLATFORM_AGNOSTIC_EOL = /\r|\n|\r\n/;

var authorsMap = {};

function usage() {
  console.log('Usage: [NODE_DEBUG=update-authors] ./node update-authors.js ' +
              ' [-a AUTHORS-FILE]');
  process.exit(1);
}

function processCmdLineArgs() {
  var cmdLineOpts = {};

  var cmdLineArgs = process.argv.splice(2);
  for (var i = 0; i < cmdLineArgs.length; ++i) {
    if (cmdLineArgs[i] === '-a') {
      if (!cmdLineArgs[++i]) {
        usage();
      } else {
        cmdLineOpts.authorsFile = cmdLineArgs[i];
      }
    } else {
      usage();
    }
  }

  return cmdLineOpts;
}

function generateAuthorsFile(authorsFilePath, cb) {
  assert(typeof authorsFilePath === 'string',
         'authorsFilePath must be a string');

  var sortedAuthorsNames = commonTools.stableSort(Object.keys(authorsMap));
  var authorsFileContent = sortedAuthorsNames.join('\n') + '\n';
  fs.writeFile(authorsFilePath, authorsFileContent, cb);
}

var latestAuthorsLineChunk = '';
function addAuthors(authors) {
  assert(typeof authors === 'string', 'authors must be a string');

  var authors = (latestAuthorsLineChunk + authors).split(PLATFORM_AGNOSTIC_EOL);
  latestAuthorsLineChunk = '';
  authors.forEach(function eachAuthor(author) {
    if (author && author.length > 0) {
      var matches = author.match(/^'([^<>]*)\s+<.*>'$/);

      if (matches) {
        var authorName = matches[1];
        debug('MATCHING: ' + authorName);
        authorsMap[authorName] = 1;
      }  else {
        debug('NOT MATCHING: ' + author);
        latestAuthorsLineChunk = author;
      }
    }
  });
}

function loadAuthorsFromGitLog(cb) {
  var gitLog = child_process.spawn('git', GIT_LOG_COMMAND_ARGS);

  gitLog.on('close', function onGitLogClose(exitCode, signal) {
    if (exitCode === 0 && signal === null) {

      debug('NB AUTHORS:' + Object.keys(authorsMap).length);

      generateAuthorsFile(AUTHORS_FILE, function onAuthorsFileGenerated(err) {
        if (err) {
          return cb(new Error('Error when generating authors file:', err));
        } else {
          console.log('Authors file generated successfully!');
        }
      });
    }
  });

  gitLog.on('error', function onGitLogError(err) {
    var errorMsg = util.format('Error when executing command %s: %s',
                               GIT_LOG_COMMAND, err);
    return cb(new Error(errorMsg));
  });

  gitLog.stdout.on('data', function onGitLogData(data) {
    addAuthors(data.toString());
  });
}

loadAuthorsFromGitLog(function done(err) {
  if (err) {
    console.error('Error when updating authors file:', err);
  } else {
    console.log('Updated authors file successfully!');
  }
});
