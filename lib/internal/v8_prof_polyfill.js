// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Node polyfill
const fs = require('fs');
const cp = require('child_process');
const os = {
  system: function(name, args) {
    if (process.platform === 'linux' && name === 'nm') {
      // Filter out vdso and vsyscall entries.
      const arg = args[args.length - 1];
      if (arg === '[vdso]' ||
          arg == '[vsyscall]' ||
          /^[0-9a-f]+-[0-9a-f]+$/.test(arg)) {
        return '';
      }
    }
    let out = cp.spawnSync(name, args).stdout.toString();
    // Auto c++filt names, but not [iItT]
    if (process.platform === 'darwin' && name === 'nm')
      out = macCppfiltNm(out);
    return out;
  }
};
const print = console.log;
function read(fileName) {
  return fs.readFileSync(fileName, 'utf8');
}
const quit = process.exit;

// Polyfill "readline()".
const logFile = arguments[arguments.length - 1];
try {
  fs.accessSync(logFile);
} catch(e) {
  console.error('Please provide a valid isolate file as the final argument.');
  process.exit(1);
}
const fd = fs.openSync(logFile, 'r');
const buf = Buffer.allocUnsafe(4096);
const dec = new (require('string_decoder').StringDecoder)('utf-8');
var line = '';
versionCheck();
function readline() {
  while (true) {
    const lineBreak = line.indexOf('\n');
    if (lineBreak !== -1) {
      const res = line.slice(0, lineBreak);
      line = line.slice(lineBreak + 1);
      return res;
    }
    const bytes = fs.readSync(fd, buf, 0, buf.length);
    line += dec.write(buf.slice(0, bytes));
    if (line.length === 0) {
      return '';
    }
  }
}

function versionCheck() {
  // v8-version looks like "v8-version,$major,$minor,$build,$patch,$candidate"
  // whereas process.versions.v8 is either "$major.$minor.$build" or
  // "$major.$minor.$build.$patch".
  var firstLine = readline();
  line = firstLine + '\n' + line;
  firstLine = firstLine.split(',');
  const curVer = process.versions.v8.split('.');
  if (firstLine.length !== 6 && firstLine[0] !== 'v8-version') {
    console.log('Unable to read v8-version from log file.');
    return;
  }
  // Compare major, minor and build; ignore the patch and candidate fields.
  for (var i = 0; i < 3; i++) {
    if (curVer[i] !== firstLine[i + 1]) {
      console.log('Testing v8 version different from logging version');
      return;
    }
  }
}

function macCppfiltNm(out) {
  // Re-grouped copy-paste from `tickprocessor.js`
  const FUNC_RE = /^([0-9a-fA-F]{8,16} [iItT] )(.*)$/gm;
  let entries = out.match(FUNC_RE);
  if (entries === null)
    return out;

  entries = entries.map((entry) => {
    return entry.replace(/^[0-9a-fA-F]{8,16} [iItT] /, '')
  });

  let filtered;
  try {
    filtered = cp.spawnSync('c++filt', [ '-p' , '-i' ], {
      input: entries.join('\n')
    }).stdout.toString();
  } catch (e) {
    return out;
  }

  let i = 0;
  filtered = filtered.split(/\n/g);
  return out.replace(FUNC_RE, (all, prefix, postfix) => {
    return prefix + (filtered[i++] || postfix);
  });
}
