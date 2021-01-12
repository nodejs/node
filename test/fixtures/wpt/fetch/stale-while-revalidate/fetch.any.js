// META: global=window,worker
// META: title=Tests Stale While Revalidate is executed for fetch API
// META: script=/common/utils.js

function wait25ms(test) {
  return new Promise(resolve => {
    test.step_timeout(() => {
      resolve();
    }, 25);
  });
}

promise_test(async (test) => {
  var request_token = token();

  const response = await fetch(`resources/stale-script.py?token=` + request_token);
  // Wait until resource is completely fetched to allow caching before next fetch.
  const body = await response.text();
  const response2 = await fetch(`resources/stale-script.py?token=` + request_token);

  assert_equals(response.headers.get('Unique-Id'), response2.headers.get('Unique-Id'));
  const body2 = await response2.text();
  assert_equals(body, body2);

  while(true) {
    const revalidation_check = await fetch(`resources/stale-script.py?query&token=` + request_token);
    if (revalidation_check.headers.get('Count') == '2') {
      break;
    }
    await wait25ms(test);
  }
}, 'Second fetch returns same response');
