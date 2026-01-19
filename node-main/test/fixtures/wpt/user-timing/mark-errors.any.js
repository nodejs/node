// If you're testing an API that constructs a PerformanceMark, add your test here.
// See the for loop below for details.
const markConstructionTests = [
  {
    testName: "Number should be rejected as the mark-options.",
    testFunction: function(newMarkFunction) {
      assert_throws_js(TypeError, function() { newMarkFunction("mark1", 123); }, "Number passed as a dict argument should cause type-error.");
    },
  },

  {
    testName: "NaN should be rejected as the mark-options.",
    testFunction: function(newMarkFunction) {
      assert_throws_js(TypeError, function() { newMarkFunction("mark1", NaN); }, "NaN passed as a dict argument should cause type-error.");
    },
  },

  {
    testName: "Infinity should be rejected as the mark-options.",
    testFunction: function(newMarkFunction) {
      assert_throws_js(TypeError, function() { newMarkFunction("mark1", Infinity); }, "Infinity passed as a dict argument should cause type-error.");
    },
  },

  {
    testName: "String should be rejected as the mark-options.",
    testFunction: function(newMarkFunction) {
      assert_throws_js(TypeError, function() { newMarkFunction("mark1", "string"); }, "String passed as a dict argument should cause type-error.")
    },
  },

  {
    testName: "Negative startTime in mark-options should be rejected",
    testFunction: function(newMarkFunction) {
      assert_throws_js(TypeError, function() { newMarkFunction("mark1", {startTime: -1}); }, "Negative startTime should cause type-error.")
    },
  },
];

// There are multiple function calls that can construct a mark using the same arguments so we run
// each test on each construction method here, avoiding duplication.
for (let testInfo of markConstructionTests) {
  test(function() {
    testInfo.testFunction(self.performance.mark);
  }, `[performance.mark]: ${testInfo.testName}`);

  test(function() {
    testInfo.testFunction((markName, obj) => new PerformanceMark(markName, obj));
  }, `[new PerformanceMark]: ${testInfo.testName}`);
}
