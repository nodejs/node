"use strict";

const common = require("../common");
const assert = require("assert");
const util = require("util");

[1, true, false, null, {}].forEach(notString => {
  common.expectsError(
    () => {
      util.deprecate(() => {}, "message", 123);
    },
    {
      code: "ERR_INVALID_ARG_TYPE",
      type: global.TypeError,
      message: `The "code" argument must be of type string`
    }
  );
});
