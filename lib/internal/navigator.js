'use strict';

const {
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
  MathClz32,
} = primordials;

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
  }
} = require('internal/errors');

const {
  getCPUs,
  getOSInformation,
  getTotalMem,
} = internalBinding('os');

class Navigator {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }
}

class InternalNavigator {}
InternalNavigator.prototype.constructor = Navigator.prototype.constructor;
ObjectSetPrototypeOf(InternalNavigator.prototype, Navigator.prototype);

function getDeviceMemory() {
  const mem = getTotalMem() / 1024 / 1024;
  if (mem <= 0.25 * 1024) return 0.25;
  if (mem >= 8 * 1024) return 8;
  const lowerBound = 1 << 31 - MathClz32(mem - 1);
  const upperBound = lowerBound * 2;
  return mem - lowerBound <= upperBound - mem ?
    lowerBound / 1024 :
    upperBound / 1024;
}

function getPlatform() {
  if (process.platform === 'win32') return 'Win32';
  if (process.platform === 'android') return 'Android';
  return getOSInformation()[0];
}

const cpuData = getCPUs();
ObjectDefineProperties(Navigator.prototype, {
  deviceMemory: {
    configurable: true,
    enumerable: true,
    value: getDeviceMemory(),
  },
  hardwareConcurrency: {
    configurable: true,
    enumerable: true,
    value: cpuData ? cpuData.length / 7 : 1,
  },
  platform: {
    configurable: true,
    enumerable: true,
    value: getPlatform(),
  },
});

module.exports = new InternalNavigator();
