// META: script=/common/subset-tests-by-key.js
// META: variant=?include=file
// META: variant=?include=javascript
// META: variant=?include=mailto
// META: variant=?exclude=(file|javascript|mailto)

// Keep this file in sync with url-setters-a-area.window.js.

promise_test(() => fetch("resources/setters_tests.json").then(res => res.json()).then(runURLSettersTests), "Loading data…");

function runURLSettersTests(all_test_cases) {
  for (var attribute_to_be_set in all_test_cases) {
    if (attribute_to_be_set == "comment") {
      continue;
    }
    var test_cases = all_test_cases[attribute_to_be_set];
    for(var i = 0, l = test_cases.length; i < l; i++) {
      var test_case = test_cases[i];
      var name = "Setting <" + test_case.href + ">." + attribute_to_be_set +
                 " = '" + test_case.new_value + "'";
      if ("comment" in test_case) {
        name += " " + test_case.comment;
      }
      const key = test_case.href.split(":")[0];
      subsetTestByKey(key, test, function() {
        var url = new URL(test_case.href);
        url[attribute_to_be_set] = test_case.new_value;
        for (var attribute in test_case.expected) {
          assert_equals(url[attribute], test_case.expected[attribute])
        }
      }, "URL: " + name)
    }
  }
}
