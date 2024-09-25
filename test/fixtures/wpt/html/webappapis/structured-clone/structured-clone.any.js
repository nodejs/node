// META: title=structuredClone() tests
// META: script=/common/sab.js
// META: script=/html/webappapis/structured-clone/structured-clone-battery-of-tests.js
// META: script=/html/webappapis/structured-clone/structured-clone-battery-of-tests-with-transferables.js
// META: script=/html/webappapis/structured-clone/structured-clone-battery-of-tests-harness.js

runStructuredCloneBatteryOfTests({
  structuredClone: (obj, transfer) => {
    return new Promise(resolve => {
      resolve(self.structuredClone(obj, { transfer }));
    });
  },
  hasDocument: typeof document !== "undefined",
});
