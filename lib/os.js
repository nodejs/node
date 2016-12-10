'use strict';

const binding = process.binding('os');
const constants = process.binding('constants').os;
const internalUtil = require('internal/util');
const isWindows = process.platform === 'win32';

exports.hostname = binding.getHostname;
exports.loadavg = binding.getLoadAvg;
exports.uptime = binding.getUptime;
exports.freemem = binding.getFreeMem;
exports.totalmem = binding.getTotalMem;
exports.cpus = binding.getCPUs;
exports.type = binding.getOSType;
exports.release = binding.getOSRelease;
exports.networkInterfaces = binding.getInterfaceAddresses;
exports.homedir = binding.getHomeDirectory;
exports.userInfo = binding.getUserInfo;

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

exports.tmpDir = internalUtil.deprecate(exports.tmpdir,
    'os.tmpDir() is deprecated. Use os.tmpdir() instead.');

exports.getNetworkInterfaces = internalUtil.deprecate(function() {
  return exports.networkInterfaces();
}, 'os.getNetworkInterfaces is deprecated. ' +
   'Use os.networkInterfaces instead.');

exports.EOL = isWindows ? '\r\n' : '\n';

if (binding.isBigEndian)
  exports.endianness = function() { return 'BE'; };
else
  exports.endianness = function() { return 'LE'; };
