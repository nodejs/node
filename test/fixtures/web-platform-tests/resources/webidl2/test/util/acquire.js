"use strict";

const { collect } = require("./collect");
const fs = require("fs");

for (const test of collect("syntax")) {
  fs.writeFileSync(test.jsonPath, `${JSON.stringify(test.ast, null, 4)}\n`)
}
