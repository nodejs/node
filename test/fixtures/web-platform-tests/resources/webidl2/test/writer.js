"use strict";

const { collect } = require("./util/collect");
const wp = require("../lib/webidl2");
const writer = require("../lib/writer");
const expect = require("expect");
const debug = true;

describe("Rewrite and parses all of the IDLs to produce the same ASTs", () => {
  for (const test of collect("syntax")) {
    it(`should produce the same AST for ${test.path}`, () => {
      try {
        const diff = test.diff(wp.parse(writer.write(test.ast), test.opt));
        if (diff && debug) console.log(JSON.stringify(diff, null, 4));
        expect(diff).toBe(undefined);
      }
      catch (e) {
        console.log(e.toString());
        throw e;
      }
    });
  }
});
