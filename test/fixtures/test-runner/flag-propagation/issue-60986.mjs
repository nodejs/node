import { describe, it } from "node:test";

describe("test", () => {
  it("calls gc", () => {
    globalThis.gc();
  });
});
