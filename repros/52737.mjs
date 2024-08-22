import { createContext, runInContext } from "vm";
runInContext(
  `
    "use strict";
    Reflect.defineProperty(globalThis, "FOO", {
      get: () => {
        throw new Error("This should not get triggered");
      },
    });
    Reflect.has(globalThis, "FOO");
  `,
  createContext(),
);