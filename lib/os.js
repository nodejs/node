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

const { safeGetenv } = internalBinding('credentials');
const constants = internalBinding('constants').os;
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
} = internalBinding('os');

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

const avgValues = new Float64Array(3);

function loadavg() {
  getLoadAvg(avgValues);
  return [avgValues[0], avgValues[1], avgValues[2]];
}

function cpus() {
  // [] is a bugfix for a regression introduced in 51cea61
  const data = getCPUs() || [];
  const result = [];
  for (var i = 0; i < data.length; i += 7) {
    result.push({
      model: data[i],
      speed: data[i + 1],
      times: {
        user: data[i + 2],
        nice: data[i + 3],
        sys: data[i + 4],
        idle: data[i + 5],
        irq: data[i + 6]
      }
    });
  }
  return result;
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
  // Count the number of bits set in parallel, which is faster than looping
  n = n - ((n >>> 1) & 0x55555555);
  n = (n & 0x33333333) + ((n >>> 2) & 0x33333333);
  return ((n + (n >>> 4) & 0xF0F0F0F) * 0x1010101) >>> 24;
}

function getCIDR(address, netmask, family) {
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
  const data = getInterfaceAddresses();
  const result = {};

  if (data === undefined)
    return result;
  for (var i = 0; i < data.length; i += 7) {
    const name = data[i];
    const entry = {
      address: data[i + 1],
      netmask: data[i + 2],
      family: data[i + 3],
      mac: data[i + 4],
      internal: data[i + 5],
      cidr: getCIDR(data[i + 1], data[i + 2], data[i + 3])
    };
    const scopeid = data[i + 6];
    if (scopeid !== -1)
      entry.scopeid = scopeid;

    const existing = result[name];
    if (existing !== undefined)
      existing.push(entry);
    else
      result[name] = [entry];
  }

  return result;
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
