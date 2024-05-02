test(() => {
  const params = new URLSearchParams("a=1&b=2&a=3");
  assert_equals(params.size, 3);

  params.delete("a");
  assert_equals(params.size, 1);
}, "URLSearchParams's size and deletion");

test(() => {
  const params = new URLSearchParams("a=1&b=2&a=3");
  assert_equals(params.size, 3);

  params.append("b", "4");
  assert_equals(params.size, 4);
}, "URLSearchParams's size and addition");

test(() => {
  const url = new URL("http://localhost/query?a=1&b=2&a=3");
  assert_equals(url.searchParams.size, 3);

  url.searchParams.delete("a");
  assert_equals(url.searchParams.size, 1);

  url.searchParams.append("b", 4);
  assert_equals(url.searchParams.size, 2);
}, "URLSearchParams's size when obtained from a URL");

test(() => {
  const url = new URL("http://localhost/query?a=1&b=2&a=3");
  assert_equals(url.searchParams.size, 3);

  url.search = "?";
  assert_equals(url.searchParams.size, 0);
}, "URLSearchParams's size when obtained from a URL and using .search");
