// META: script=/common/subset-tests-by-key.js
// META: variant=?include=file
// META: variant=?include=javascript
// META: variant=?include=mailto
// META: variant=?exclude=(file|javascript|mailto)

// Keep this file in sync with url-setters.any.js.

promise_test(() => fetch("resources/setters_tests.json").then(res => res.json()).then(runURLSettersTests), "Loading data…");

function runURLSettersTests(allTestCases) {
  for (const [propertyToBeSet, testCases] of Object.entries(allTestCases)) {
    if (propertyToBeSet === "comment") {
      continue;
    }

    for (const testCase of testCases) {
      const name = `Setting <${testCase.href}>.${propertyToBeSet} = '${testCase.new_value}'${
        testCase.comment ? ` ${testCase.comment}` : ''
      }`;

      const key = testCase.href.split(":")[0];
      subsetTestByKey(key, test, () => {
        const url = document.createElement("a");
        url.href = testCase.href;
        url[propertyToBeSet] = testCase.new_value;

        for (const [property, expectedValue] of Object.entries(testCase.expected)) {
          assert_equals(url[property], expectedValue);
        }
      }, `<a>: ${name}`);

      subsetTestByKey(key, test, () => {
        const url = document.createElement("area");
        url.href = testCase.href;
        url[propertyToBeSet] = testCase.new_value;

        for (const [property, expectedValue] of Object.entries(testCase.expected)) {
          assert_equals(url[property], expectedValue);
        }
      }, `<area>: ${name}`);
    }
  }
}
