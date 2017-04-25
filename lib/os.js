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

const pushValToArrayMax = process.binding('util').pushValToArrayMax;
const constants = process.binding('constants').os;
const deprecate = require('internal/util').deprecate;
const isWindows = process.platform === 'win32';

const {
  getCPUs,
  getFreeMem,
  getHomeDirectory,
  getHostname,
  getInterfaceAddresses,
  getLoadAvg,
  getOSRelease,
  getOSType,
  getTotalMem,
  getUserInfo,
  getUptime,
  isBigEndian
} = process.binding('os');

getFreeMem[Symbol.toPrimitive] = () => getFreeMem();
getHostname[Symbol.toPrimitive] = () => getHostname();
getHomeDirectory[Symbol.toPrimitive] = () => getHomeDirectory();
getOSRelease[Symbol.toPrimitive] = () => getOSRelease();
getOSType[Symbol.toPrimitive] = () => getOSType();
getTotalMem[Symbol.toPrimitive] = () => getTotalMem();
getUptime[Symbol.toPrimitive] = () => getUptime();

const kEndianness = isBigEndian ? 'BE' : 'LE';

const tmpDirDeprecationMsg =
  'os.tmpDir() is deprecated. Use os.tmpdir() instead.';

const getNetworkInterfacesDepMsg =
  'os.getNetworkInterfaces is deprecated. Use os.networkInterfaces instead.';

const avgValues = new Float64Array(3);
const cpuValues = new Float64Array(6 * pushValToArrayMax);

function loadavg() {
  getLoadAvg(avgValues);
  return [avgValues[0], avgValues[1], avgValues[2]];
}

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

function cpus() {
  return getCPUs(addCPUInfo, cpuValues, []);
}

function arch() {
  return process.arch;
}
arch[Symbol.toPrimitive] = () => process.arch;

function platform() {
  return process.platform;
}
platform[Symbol.toPrimitive] = () => process.platform;

function tmpdir() {
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
}
tmpdir[Symbol.toPrimitive] = () => tmpdir();

function endianness() {
  return kEndianness;
}
endianness[Symbol.toPrimitive] = () => kEndianness;

module.exports = exports = {
  arch,
  cpus,
  EOL: isWindows ? '\r\n' : '\n',
  endianness,
  freemem: getFreeMem,
  homedir: getHomeDirectory,
  hostname: getHostname,
  loadavg,
  networkInterfaces: getInterfaceAddresses,
  platform,
  release: getOSRelease,
  tmpdir,
  totalmem: getTotalMem,
  type: getOSType,
  userInfo: getUserInfo,
  uptime: getUptime,

  // Deprecated APIs
  getNetworkInterfaces: deprecate(getInterfaceAddresses,
                                  getNetworkInterfacesDepMsg,
                                  'DEP0023'),
  tmpDir: deprecate(tmpdir, tmpDirDeprecationMsg, 'DEP0022')
};

Object.defineProperty(module.exports, 'constants', {
  configurable: false,
  enumerable: true,
  value: constants
});
