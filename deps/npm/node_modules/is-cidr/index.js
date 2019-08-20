"use strict";
const cidrRegex = require("cidr-regex");
const re4 = cidrRegex.v4({exact: true});
const re6 = cidrRegex.v6({exact: true});

const isCidr = module.exports = str => {
  if (re4.test(str)) return 4;
  if (re6.test(str)) return 6;
  return 0;
};

isCidr.v4 = str => re4.test(str);
isCidr.v6 = str => re6.test(str);
