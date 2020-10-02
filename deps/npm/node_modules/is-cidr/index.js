"use strict";
const {v4, v6} = require("cidr-regex");

const re4 = v4({exact: true});
const re6 = v6({exact: true});

module.exports = str => re4.test(str) ? 4 : (re6.test(str) ? 6 : 0);
module.exports.v4 = str => re4.test(str);
module.exports.v6 = str => re6.test(str);
