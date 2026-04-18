function assert_prop_desc_equals(object, property_key, expected) {
  let actual = Object.getOwnPropertyDescriptor(object, property_key);
  if (expected === undefined) {
    assert_equals(
        actual, undefined,
        "(assert_prop_desc_equals: no property descriptor expected)");
    return;
  }
  for (p in actual) {
    assert_true(
        expected.hasOwnProperty(p),
        "(assert_prop_desc_equals: property '" + p + "' is not expected)");
    assert_equals(
        actual[p], expected[p],
        "(assert_prop_desc_equals: property '" + p + "')");
  }
  for (p in expected) {
    assert_true(
        actual.hasOwnProperty(p),
        "(assert_prop_desc_equals: expected property '" + p + "' missing)");
  }
}
