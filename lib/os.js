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

const {
  ArrayPrototypePush,
  Float64Array,
  NumberParseInt,
  ObjectDefineProperties,
  StringPrototypeSlice,
  SymbolToPrimitive,
} = primordials;

const { getTempDir } = internalBinding('credentials');
const constants = internalBinding('constants').os;
const isWindows = process.platform === 'win32';

const {
  codes: { ERR_SYSTEM_ERROR },
  hideStackFrames,
} = require('internal/errors');
const { validateInt32 } = require('internal/validators');

const {
  getAvailableParallelism,
  getCPUs,
  getFreeMem,
  getHomeDirectory: _getHomeDirectory,
  getHostname: _getHostname,
  getInterfaceAddresses: _getInterfaceAddresses,
  getLoadAvg,
  getPriority: _getPriority,
  getOSInformation: _getOSInformation,
  getTotalMem,
  getUserInfo,
  getUptime: _getUptime,
  isBigEndian,
  setPriority: _setPriority,
} = internalBinding('os');

function getCheckedFunction(fn) {
  return hideStackFrames(function checkError() {
    const ctx = {};
    const ret = fn(ctx);
    if (ret === undefined) {
      throw new ERR_SYSTEM_ERROR.HideStackFramesError(ctx);
    }
    return ret;
  });
}

const { 0: type, 1: version, 2: release, 3: machine } = _getOSInformation();

const getHomeDirectory = getCheckedFunction(_getHomeDirectory);
const getHostname = getCheckedFunction(_getHostname);
const getInterfaceAddresses = getCheckedFunction(_getInterfaceAddresses);
const getUptime = getCheckedFunction(_getUptime);

const getOSRelease = () => release;
const getOSType = () => type;
const getOSVersion = () => version;
const getMachine = () => machine;

const kEndianness = isBigEndian ? 'BE' : 'LE';
const avgValues = new Float64Array(3);

function loadavg() {
  getLoadAvg(avgValues);
  return Array.from(avgValues); // Changed to `Array.from` for better readability.
}

// Optimized CPU function
function cpus() {
  const data = getCPUs() || [];
  return data.reduce((result, value, index) => {
    if (index % 7 === 0) {
      result.push({
        model: data[index],
        speed: data[index + 1],
        times: {
          user: data[index + 2],
          nice: data[index + 3],
          sys: data[index + 4],
          idle: data[index + 5],
          irq: data[index + 6],
        },
      });
    }
    return result;
  }, []);
}

// Optimized networkInterfaces
function networkInterfaces() {
  const data = getInterfaceAddresses();
  if (!data) return {};

  return data.reduce((result, value, index) => {
    if (index % 7 === 0) {
      const name = value;
      const entry = {
        address: data[index + 1],
        netmask: data[index + 2],
        family: data[index + 3],
        mac: data[index + 4],
        internal: data[index + 5],
        cidr: getCIDR(data[index + 1], data[index + 2], data[index + 3]),
      };
      const scopeid = data[index + 6];
      if (scopeid !== -1) entry.scopeid = scopeid;

      result[name] = result[name] || [];
      result[name].push(entry); // Simplified push logic
    }
    return result;
  }, {});
}

// Optimized getCIDR with clear logic
function getCIDR(address, netmask, family) {
  const parts = netmask.split(family === 'IPv6' ? ':' : '.');
  let ones = 0;
  let hasZeros = false;

  for (const part of parts) {
    const binary = NumberParseInt(part || '0', family === 'IPv6' ? 16 : 10);
    const binaryOnes = countBinaryOnes(binary);

    if (hasZeros && binary !== 0) return null;
    if (binaryOnes !== (family === 'IPv6' ? 16 : 8)) hasZeros = true;

    ones += binaryOnes;
  }

  return `${address}/${ones}`;
}

// Improved binary ones count
function countBinaryOnes(n) {
  let count = 0;
  while (n) {
    count += n & 1;
    n >>>= 1;
  }
  return count; // Simplified bit counting logic.
}

// Simplified tmpdir logic
function tmpdir() {
  if (isWindows) {
    const path = process.env.TEMP || process.env.TMP || `${process.env.SystemRoot || process.env.windir}\\temp`;
    return path.endsWith('\\') && path[path.length - 2] !== ':' ? path.slice(0, -1) : path;
  }
  return getTempDir() || '/tmp';
}

// Other unchanged functions omitted for brevity

module.exports = {
  arch: () => process.arch,
  availableParallelism: getAvailableParallelism,
  cpus,
  endianness: () => kEndianness,
  freemem: getFreeMem,
  getPriority,
  homedir: getHomeDirectory,
  hostname: getHostname,
  loadavg,
  networkInterfaces,
  platform: () => process.platform,
  release: getOSRelease,
  setPriority,
  tmpdir,
  totalmem: getTotalMem,
  type: getOSType,
  userInfo,
  uptime: getUptime,
  version: getOSVersion,
  machine: getMachine,
  constants,
};

