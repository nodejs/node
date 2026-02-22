var priorities = ["high",
                  "low",
                  "auto"
                 ];

for (idx in priorities) {
  test(() => {
    new Request("", {priority: priorities[idx]});
  }, "new Request() with a '"  + priorities[idx] + "' priority does not throw an error");
}

test(() => {
  assert_throws_js(TypeError, () => {
    new Request("", {priority: 'invalid'});
  }, "a new Request() must throw a TypeError if RequestInit's priority is an invalid value");
}, "new Request() throws a TypeError if any of RequestInit's members' values are invalid");

for (idx in priorities) {
  promise_test(function(t) {
    return fetch('hello.txt', { priority: priorities[idx] });
  }, "fetch() with a '"  + priorities[idx] + "' priority completes successfully");
}

promise_test(function(t) {
  return promise_rejects_js(t, TypeError, fetch('hello.txt', { priority: 'invalid' }));
}, "fetch() with an invalid priority returns a rejected promise with a TypeError");
