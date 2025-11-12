(function() {
  var subTestStart = 0;
  var subTestEnd = Infinity;
  var match;
  if (location.search) {
    match = /(?:^\?|&)(\d+)-(\d+|last)(?:&|$)/.exec(location.search);
    if (match) {
      subTestStart = parseInt(match[1], 10);
      if (match[2] !== "last") {
          subTestEnd = parseInt(match[2], 10);
      }
    }
    // Below is utility code to generate <meta> for copy/paste into tests.
    // Sample usage:
    // test.html?split=1000
    match = /(?:^\?|&)split=(\d+)(?:&|$)/.exec(location.search);
    if (match) {
      var testsPerVariant = parseInt(match[1], 10);
      add_completion_callback(tests => {
        var total = tests.length;
        var template = '<meta name="variant" content="?%s-%s">';
        var metas = [];
        for (var i = 1; i < total - testsPerVariant; i = i + testsPerVariant) {
          metas.push(template.replace("%s", i).replace("%s", i + testsPerVariant - 1));
        }
        metas.push(template.replace("%s", i).replace("%s", "last"));
        var pre = document.createElement('pre');
        pre.textContent = metas.join('\n');
        document.body.insertBefore(pre, document.body.firstChild);
        document.getSelection().selectAllChildren(pre);
      });
    }
  }
  /**
   * Check if `currentSubTest` is in the subset specified in the URL.
   * @param {number} currentSubTest
   * @returns {boolean}
   */
  function shouldRunSubTest(currentSubTest) {
    return currentSubTest >= subTestStart && currentSubTest <= subTestEnd;
  }
  var currentSubTest = 0;
  /**
   * Only test a subset of tests with, e.g., `?1-10` in the URL.
   * Can be used together with `<meta name="variant" content="...">`
   * Sample usage:
   * for (const test of tests) {
   *   subsetTest(async_test, test.fn, test.name);
   * }
   */
  function subsetTest(testFunc, ...args) {
    currentSubTest++;
    if (shouldRunSubTest(currentSubTest)) {
      return testFunc(...args);
    }
    return null;
  }
  self.shouldRunSubTest = shouldRunSubTest;
  self.subsetTest = subsetTest;
})();
