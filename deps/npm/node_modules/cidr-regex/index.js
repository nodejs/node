"use strict";

const ipRegex = require("ip-regex");

const defaultOpts = {exact: false};

const v4str = `${ipRegex.v4().source}\\/(3[0-2]|[12]?[0-9])`;
const v6str = `${ipRegex.v6().source}\\/(12[0-8]|1[01][0-9]|[1-9]?[0-9])`;

// can not precompile the non-exact regexes because global flag makes the regex object stateful
// which would require the user to reset .lastIndex on subsequent calls
const v4exact = new RegExp(`^${v4str}$`);
const v6exact = new RegExp(`^${v6str}$`);
const v46exact = new RegExp(`(?:^${v4str}$)|(?:^${v6str}$)`);

module.exports = ({exact} = defaultOpts) => exact ? v46exact : new RegExp(`(?:${v4str})|(?:${v6str})`, "g");
module.exports.v4 = ({exact} = defaultOpts) => exact ? v4exact : new RegExp(v4str, "g");
module.exports.v6 = ({exact} = defaultOpts) => exact ? v6exact : new RegExp(v6str, "g");
