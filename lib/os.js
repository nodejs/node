'use strict';

const binding = process.binding('os');
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


exports.arch = function() {
  return process.arch;
};

exports.platform = function() {
  return process.platform;
};

const trailingSlashRe = isWindows ? /[^:]\\$/
                                  : /.\/$/;

var tmpdir;

exports.tmpdir = function() {
  if (!tmpdir) {
    if (isWindows) {
      tmpdir = process.env.TEMP ||
             process.env.TMP ||
             (process.env.SystemRoot || process.env.windir) + '\\temp';
    } else {
      tmpdir = process.env.TMPDIR ||
             process.env.TMP ||
             process.env.TEMP ||
             '/tmp';
    }

    if (trailingSlashRe.test(tmpdir))
      tmpdir = tmpdir.slice(0, -1);
  }

  return tmpdir;
};

exports.tmpDir = exports.tmpdir;

exports.getNetworkInterfaces = internalUtil.deprecate(function() {
  return exports.networkInterfaces();
}, 'os.getNetworkInterfaces is deprecated. ' +
   'Use os.networkInterfaces instead.');

exports.EOL = isWindows ? '\r\n' : '\n';

if (binding.isBigEndian)
  exports.endianness = function() { return 'BE'; };
else
  exports.endianness = function() { return 'LE'; };
