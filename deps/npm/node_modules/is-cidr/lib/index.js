'use strict';

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.isCidrV6 = exports.isCidrV4 = undefined;

var _cidrRegex = require('cidr-regex');

var isCidrV4 = exports.isCidrV4 = function isCidrV4(str) {
  return _cidrRegex.cidrv4.test(str);
};

var isCidrV6 = exports.isCidrV6 = function isCidrV6(str) {
  return _cidrRegex.cidrv6.test(str);
};

exports.default = isCidrV4;