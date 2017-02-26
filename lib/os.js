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
exports.tmpDir = internalUtil.deprecate(exports.tmpdir, tmpDirDeprecationMsg);

exports.getNetworkInterfaces = internalUtil.deprecate(function() {
  return exports.networkInterfaces();
}, 'os.getNetworkInterfaces is deprecated. ' +
   'Use os.networkInterfaces instead.');

exports.EOL = isWindows ? '\r\n' : '\n';

if (binding.isBigEndian)
  exports.endianness = function() { return 'BE'; };
else
  exports.endianness = function() { return 'LE'; };
