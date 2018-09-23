"use strict";
const cidrRegex = require("cidr-regex");

const isCidr = module.exports = string => cidrRegex({exact: true}).test(string);
isCidr.v4 = string => cidrRegex.v4({exact: true}).test(string);
isCidr.v6 = string => cidrRegex.v6({exact: true}).test(string);
