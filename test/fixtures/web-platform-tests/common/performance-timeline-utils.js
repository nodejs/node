var performanceNamespace = window.performance;
var namespace_check = false;
function wp_test(func, msg, properties)
{
  // only run the namespace check once
  if (!namespace_check)
  {
    namespace_check = true;

    if (performanceNamespace === undefined || performanceNamespace == null)
    {
      // show a single error that window.performance is undefined
      test(function() { assert_true(performanceNamespace !== undefined && performanceNamespace != null, "window.performance is defined and not null"); }, "window.performance is defined and not null.", {author:"W3C http://www.w3.org/",help:"http://www.w3.org/TR/navigation-timing/#sec-window.performance-attribute",assert:"The window.performance attribute provides a hosting area for performance related attributes. "});
    }
  }

  test(func, msg, properties);
}

function test_true(value, msg, properties)
{
  wp_test(function () { assert_true(value, msg); }, msg, properties);
}

function test_equals(value, equals, msg, properties)
{
  wp_test(function () { assert_equals(value, equals, msg); }, msg, properties);
}

// assert for every entry in `expectedEntries`, there is a matching entry _somewhere_ in `actualEntries`
function test_entries(actualEntries, expectedEntries) {
  test_equals(actualEntries.length, expectedEntries.length)
  expectedEntries.forEach(function (expectedEntry) {
    var foundEntry = actualEntries.find(function (actualEntry) {
      return typeof Object.keys(expectedEntry).find(function (key) {
            return actualEntry[key] !== expectedEntry[key]
          }) === 'undefined'
    })
    test_true(!!foundEntry, `Entry ${JSON.stringify(expectedEntry)} could not be found.`)
    if (foundEntry) {
      assert_object_equals(foundEntry.toJSON(), expectedEntry)
    }
  })
}
