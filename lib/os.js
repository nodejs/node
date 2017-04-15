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

'use strict';

const binding = process.binding('os');
const getCPUs = binding.getCPUs;
const getLoadAvg = binding.getLoadAvg;
const pushValToArrayMax = process.binding('util').pushValToArrayMax;
const constants = process.binding('constants').os;
const internalUtil = require('internal/util');
const isWindows = process.platform === 'win32';

exports.hostname = binding.getHostname;
exports.uptime = binding.getUptime;
exports.freemem = binding.getFreeMem;
exports.totalmem = binding.getTotalMem;
exports.type = binding.getOSType;
exports.release = binding.getOSRelease;
exports.networkInterfaces = binding.getInterfaceAddresses;
exports.homedir = binding.getHomeDirectory;
exports.userInfo = binding.getUserInfo;

const avgValues = new Float64Array(3);
exports.loadavg = function loadavg() {
  getLoadAvg(avgValues);
  return [avgValues[0], avgValues[1], avgValues[2]];
};

const cpuValues = new Float64Array(6 * pushValToArrayMax);
function addCPUInfo() {
  for (var i = 0, c = 0; i < arguments.length; ++i, c += 6) {
    this[this.length] = {
      model: arguments[i],
      speed: cpuValues[c],
      times: {
        user: cpuValues[c + 1],
        nice: cpuValues[c + 2],
        sys: cpuValues[c + 3],
        idle: cpuValues[c + 4],
        irq: cpuValues[c + 5]
      }
    };
  }
}
exports.cpus = function cpus() {
  return getCPUs(addCPUInfo, cpuValues, []);
};

Object.defineProperty(exports, 'constants', {
  configurable: false,
  enumerable: true,
  value: constants
});

exports.arch = function() {
  return process.arch;
};

exports.platform = function() {
  return process.platform;
};

exports.tmpdir = function() {
  var path;
  if (isWindows) {
    path = process.env.TEMP ||
           process.env.TMP ||
           (process.env.SystemRoot || process.env.windir) + '\\temp';
    if (path.length > 1 && path.endsWith('\\') && !path.endsWith(':\\'))
      path = path.slice(0, -1);
  } else {
    path = process.env.TMPDIR ||
           process.env.TMP ||
           process.env.TEMP ||
           '/tmp';
    if (path.length > 1 && path.endsWith('/'))
      path = path.slice(0, -1);
  }

  return path;
};

const tmpDirDeprecationMsg =
  'os.tmpDir() is deprecated. Use os.tmpdir() instead.';
exports.tmpDir = internalUtil.deprecate(exports.tmpdir,
                                        tmpDirDeprecationMsg,
                                        'DEP0022');

exports.getNetworkInterfaces = internalUtil.deprecate(function() {
  return exports.networkInterfaces();
}, 'os.getNetworkInterfaces is deprecated. ' +
   'Use os.networkInterfaces instead.', 'DEP0023');

exports.EOL = isWindows ? '\r\n' : '\n';

if (binding.isBigEndian)
  exports.endianness = function() { return 'BE'; };
else
  exports.endianness = function() { return 'LE'; };
