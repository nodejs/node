// META: global=window,worker

function runTests(data) {
  for (let entry of data) {
    test(function () {
      const pattern = new URLPattern(entry.pattern);

      if (entry.expected === null) {
        assert_throws_js(TypeError, _ => pattern.generate(entry.component, entry.groups),
                         'generate() should fail with TypeError');
        return;
      }

      const result = pattern.generate(entry.component, entry.groups);
      assert_equals(result, entry.expected);
    }, `Pattern: ${JSON.stringify(entry.pattern)} ` +
    `Component: ${entry.component} ` +
    `Groups: ${JSON.stringify(entry.groups)}`);
  }
}

promise_test(async function () {
  const response = await fetch('resources/urlpattern-generate-test-data.json');
  const data = await response.json();
  runTests(data);
}, 'Loading data...');
