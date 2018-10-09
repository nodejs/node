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

const { pushValToArrayMax, safeGetenv } = internalBinding('util');
const constants = process.binding('constants').os;
const { deprecate } = require('internal/util');
const isWindows = process.platform === 'win32';

const { codes: { ERR_SYSTEM_ERROR } } = require('internal/errors');
const { validateInt32 } = require('internal/validators');

const {
  getCPUs,
  getFreeMem,
  getHomeDirectory: _getHomeDirectory,
  getHostname: _getHostname,
  getInterfaceAddresses: _getInterfaceAddresses,
  getLoadAvg,
  getOSRelease: _getOSRelease,
  getOSType: _getOSType,
  getPriority: _getPriority,
  getTotalMem,
  getUserInfo,
  getUptime,
  isBigEndian,
  setPriority: _setPriority
} = process.binding('os');

function getCheckedFunction(fn) {
  return function checkError(...args) {
    const ctx = {};
    const ret = fn(...args, ctx);
    if (ret === undefined) {
      const err = new ERR_SYSTEM_ERROR(ctx);
      Error.captureStackTrace(err, checkError);
      throw err;
    }
    return ret;
  };
}

const getHomeDirectory = getCheckedFunction(_getHomeDirectory);
const getHostname = getCheckedFunction(_getHostname);
const getInterfaceAddresses = getCheckedFunction(_getInterfaceAddresses);
const getOSRelease = getCheckedFunction(_getOSRelease);
const getOSType = getCheckedFunction(_getOSType);

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
    path = safeGetenv('TMPDIR') ||
           safeGetenv('TMP') ||
           safeGetenv('TEMP') ||
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

// Returns the number of ones in the binary representation of the decimal
// number.
function countBinaryOnes(n) {
  let count = 0;
  // Remove one "1" bit from n until n is the power of 2. This iterates k times
  // while k is the number of "1" in the binary representation.
  // For more check https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Bitwise_Operators
  while (n !== 0) {
    n = n & (n - 1);
    count++;
  }
  return count;
}

function getCIDR({ address, netmask, family }) {
  let ones = 0;
  let split = '.';
  let range = 10;
  let groupLength = 8;
  let hasZeros = false;

  if (family === 'IPv6') {
    split = ':';
    range = 16;
    groupLength = 16;
  }

  const parts = netmask.split(split);
  for (var i = 0; i < parts.length; i++) {
    if (parts[i] !== '') {
      const binary = parseInt(parts[i], range);
      const tmp = countBinaryOnes(binary);
      ones += tmp;
      if (hasZeros) {
        if (tmp !== 0) {
          return null;
        }
      } else if (tmp !== groupLength) {
        if ((binary & 1) !== 0) {
          return null;
        }
        hasZeros = true;
      }
    }
  }

  return `${address}/${ones}`;
}

function networkInterfaces() {
  const interfaceAddresses = getInterfaceAddresses();

  const keys = Object.keys(interfaceAddresses);
  for (var i = 0; i < keys.length; i++) {
    const arr = interfaceAddresses[keys[i]];
    for (var j = 0; j < arr.length; j++) {
      arr[j].cidr = getCIDR(arr[j]);
    }
  }

  return interfaceAddresses;
}

function setPriority(pid, priority) {
  if (priority === undefined) {
    priority = pid;
    pid = 0;
  }

  validateInt32(pid, 'pid');
  validateInt32(priority, 'priority', -20, 19);

  const ctx = {};

  if (_setPriority(pid, priority, ctx) !== 0)
    throw new ERR_SYSTEM_ERROR(ctx);
}

function getPriority(pid) {
  if (pid === undefined)
    pid = 0;
  else
    validateInt32(pid, 'pid');

  const ctx = {};
  const priority = _getPriority(pid, ctx);

  if (priority === undefined)
    throw new ERR_SYSTEM_ERROR(ctx);

  return priority;
}

function userInfo(options) {
  if (typeof options !== 'object')
    options = null;

  const ctx = {};
  const user = getUserInfo(options, ctx);

  if (user === undefined)
    throw new ERR_SYSTEM_ERROR(ctx);

  return user;
}

module.exports = {
  arch,
  cpus,
  endianness,
  freemem: getFreeMem,
  getPriority,
  homedir: getHomeDirectory,
  hostname: getHostname,
  loadavg,
  networkInterfaces,
  platform,
  release: getOSRelease,
  setPriority,
  tmpdir,
  totalmem: getTotalMem,
  type: getOSType,
  userInfo,
  uptime: getUptime,

  // Deprecated APIs
  getNetworkInterfaces: deprecate(getInterfaceAddresses,
                                  getNetworkInterfacesDepMsg,
                                  'DEP0023'),
  tmpDir: deprecate(tmpdir, tmpDirDeprecationMsg, 'DEP0022')
};

Object.defineProperties(module.exports, {
  constants: {
    configurable: false,
    enumerable: true,
    value: constants
  },

  EOL: {
    configurable: true,
    enumerable: true,
    writable: false,
    value: isWindows ? '\r\n' : '\n'
  }
});
