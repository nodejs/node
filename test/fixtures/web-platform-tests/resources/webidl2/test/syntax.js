"use strict";

const { collect } = require("./util/collect");
const expect = require("expect");
const debug = true;

describe("Parses all of the IDLs to produce the correct ASTs", () => {
  for (const test of collect("syntax")) {
    it(`should produce the same AST for ${test.path}`, () => {
      try {
        expect(test.diff()).toBeFalsy();
      }
      catch (e) {
        console.log(e.toString());
        throw e;
      }
    });
  }
});
