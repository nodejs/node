// META: title=Headers set-cookie special cases
// META: global=window,worker

const headerList = [
  ["set-cookie", "foo=bar"],
  ["Set-Cookie", "fizz=buzz; domain=example.com"],
];

const setCookie2HeaderList = [
  ["set-cookie2", "foo2=bar2"],
  ["Set-Cookie2", "fizz2=buzz2; domain=example2.com"],
];

function assert_nested_array_equals(actual, expected) {
  assert_equals(actual.length, expected.length, "Array length is not equal");
  for (let i = 0; i < expected.length; i++) {
    assert_array_equals(actual[i], expected[i]);
  }
}

test(function () {
  const headers = new Headers(headerList);
  assert_equals(
    headers.get("set-cookie"),
    "foo=bar, fizz=buzz; domain=example.com",
  );
}, "Headers.prototype.get combines set-cookie headers in order");

test(function () {
  const headers = new Headers(headerList);
  const list = [...headers];
  assert_nested_array_equals(list, [
    ["set-cookie", "foo=bar"],
    ["set-cookie", "fizz=buzz; domain=example.com"],
  ]);
}, "Headers iterator does not combine set-cookie headers");

test(function () {
  const headers = new Headers(setCookie2HeaderList);
  const list = [...headers];
  assert_nested_array_equals(list, [
    ["set-cookie2", "foo2=bar2, fizz2=buzz2; domain=example2.com"],
  ]);
}, "Headers iterator does not special case set-cookie2 headers");

test(function () {
  const headers = new Headers([...headerList, ...setCookie2HeaderList]);
  const list = [...headers];
  assert_nested_array_equals(list, [
    ["set-cookie", "foo=bar"],
    ["set-cookie", "fizz=buzz; domain=example.com"],
    ["set-cookie2", "foo2=bar2, fizz2=buzz2; domain=example2.com"],
  ]);
}, "Headers iterator does not combine set-cookie & set-cookie2 headers");

test(function () {
  // Values are in non alphabetic order, and the iterator should yield in the
  // headers in the exact order of the input.
  const headers = new Headers([
    ["set-cookie", "z=z"],
    ["set-cookie", "a=a"],
    ["set-cookie", "n=n"],
  ]);
  const list = [...headers];
  assert_nested_array_equals(list, [
    ["set-cookie", "z=z"],
    ["set-cookie", "a=a"],
    ["set-cookie", "n=n"],
  ]);
}, "Headers iterator preserves set-cookie ordering");

test(
  function () {
    const headers = new Headers([
      ["xylophone-header", "1"],
      ["best-header", "2"],
      ["set-cookie", "3"],
      ["a-cool-header", "4"],
      ["set-cookie", "5"],
      ["a-cool-header", "6"],
      ["best-header", "7"],
    ]);
    const list = [...headers];
    assert_nested_array_equals(list, [
      ["a-cool-header", "4, 6"],
      ["best-header", "2, 7"],
      ["set-cookie", "3"],
      ["set-cookie", "5"],
      ["xylophone-header", "1"],
    ]);
  },
  "Headers iterator preserves per header ordering, but sorts keys alphabetically",
);

test(
  function () {
    const headers = new Headers([
      ["xylophone-header", "7"],
      ["best-header", "6"],
      ["set-cookie", "5"],
      ["a-cool-header", "4"],
      ["set-cookie", "3"],
      ["a-cool-header", "2"],
      ["best-header", "1"],
    ]);
    const list = [...headers];
    assert_nested_array_equals(list, [
      ["a-cool-header", "4, 2"],
      ["best-header", "6, 1"],
      ["set-cookie", "5"],
      ["set-cookie", "3"],
      ["xylophone-header", "7"],
    ]);
  },
  "Headers iterator preserves per header ordering, but sorts keys alphabetically (and ignores value ordering)",
);

test(function () {
  const headers = new Headers([["fizz", "buzz"], ["X-Header", "test"]]);
  const iterator = headers[Symbol.iterator]();
  assert_array_equals(iterator.next().value, ["fizz", "buzz"]);
  headers.append("Set-Cookie", "a=b");
  assert_array_equals(iterator.next().value, ["set-cookie", "a=b"]);
  headers.append("Accept", "text/html");
  assert_array_equals(iterator.next().value, ["set-cookie", "a=b"]);
  assert_array_equals(iterator.next().value, ["x-header", "test"]);
  headers.append("set-cookie", "c=d");
  assert_array_equals(iterator.next().value, ["x-header", "test"]);
  assert_true(iterator.next().done);
}, "Headers iterator is correctly updated with set-cookie changes");

test(function () {
  const headers = new Headers([
    ["set-cookie", "a"],
    ["set-cookie", "b"],
    ["set-cookie", "c"]
  ]);
  const iterator = headers[Symbol.iterator]();
  assert_array_equals(iterator.next().value, ["set-cookie", "a"]);
  headers.delete("set-cookie");
  headers.append("set-cookie", "d");
  headers.append("set-cookie", "e");
  headers.append("set-cookie", "f");
  assert_array_equals(iterator.next().value, ["set-cookie", "e"]);
  assert_array_equals(iterator.next().value, ["set-cookie", "f"]);
  assert_true(iterator.next().done);
}, "Headers iterator is correctly updated with set-cookie changes #2");

test(function () {
  const headers = new Headers(headerList);
  assert_true(headers.has("sEt-cOoKiE"));
}, "Headers.prototype.has works for set-cookie");

test(function () {
  const headers = new Headers(setCookie2HeaderList);
  headers.append("set-Cookie", "foo=bar");
  headers.append("sEt-cOoKiE", "fizz=buzz");
  const list = [...headers];
  assert_nested_array_equals(list, [
    ["set-cookie", "foo=bar"],
    ["set-cookie", "fizz=buzz"],
    ["set-cookie2", "foo2=bar2, fizz2=buzz2; domain=example2.com"],
  ]);
}, "Headers.prototype.append works for set-cookie");

test(function () {
  const headers = new Headers(headerList);
  headers.set("set-cookie", "foo2=bar2");
  const list = [...headers];
  assert_nested_array_equals(list, [
    ["set-cookie", "foo2=bar2"],
  ]);
}, "Headers.prototype.set works for set-cookie");

test(function () {
  const headers = new Headers(headerList);
  headers.delete("set-Cookie");
  const list = [...headers];
  assert_nested_array_equals(list, []);
}, "Headers.prototype.delete works for set-cookie");

test(function () {
  const headers = new Headers();
  assert_array_equals(headers.getSetCookie(), []);
}, "Headers.prototype.getSetCookie with no headers present");

test(function () {
  const headers = new Headers([headerList[0]]);
  assert_array_equals(headers.getSetCookie(), ["foo=bar"]);
}, "Headers.prototype.getSetCookie with one header");

test(function () {
  const headers = new Headers({ "Set-Cookie": "foo=bar" });
  assert_array_equals(headers.getSetCookie(), ["foo=bar"]);
}, "Headers.prototype.getSetCookie with one header created from an object");

test(function () {
  const headers = new Headers(headerList);
  assert_array_equals(headers.getSetCookie(), [
    "foo=bar",
    "fizz=buzz; domain=example.com",
  ]);
}, "Headers.prototype.getSetCookie with multiple headers");

test(function () {
  const headers = new Headers([["set-cookie", ""]]);
  assert_array_equals(headers.getSetCookie(), [""]);
}, "Headers.prototype.getSetCookie with an empty header");

test(function () {
  const headers = new Headers([["set-cookie", "x"], ["set-cookie", "x"]]);
  assert_array_equals(headers.getSetCookie(), ["x", "x"]);
}, "Headers.prototype.getSetCookie with two equal headers");

test(function () {
  const headers = new Headers([
    ["set-cookie2", "x"],
    ["set-cookie", "y"],
    ["set-cookie2", "z"],
  ]);
  assert_array_equals(headers.getSetCookie(), ["y"]);
}, "Headers.prototype.getSetCookie ignores set-cookie2 headers");

test(function () {
  // Values are in non alphabetic order, and the iterator should yield in the
  // headers in the exact order of the input.
  const headers = new Headers([
    ["set-cookie", "z=z"],
    ["set-cookie", "a=a"],
    ["set-cookie", "n=n"],
  ]);
  assert_array_equals(headers.getSetCookie(), ["z=z", "a=a", "n=n"]);
}, "Headers.prototype.getSetCookie preserves header ordering");

test(function () {
  const headers = new Headers({"Set-Cookie": "  a=b\n"});
  headers.append("set-cookie", "\n\rc=d  ");
  assert_nested_array_equals([...headers], [
    ["set-cookie", "a=b"],
    ["set-cookie", "c=d"]
  ]);
  headers.set("set-cookie", "\te=f ");
  assert_nested_array_equals([...headers], [["set-cookie", "e=f"]]);
}, "Adding Set-Cookie headers normalizes their value");

test(function () {
  assert_throws_js(TypeError, () => {
    new Headers({"set-cookie": "\0"});
  });

  const headers = new Headers();
  assert_throws_js(TypeError, () => {
    headers.append("Set-Cookie", "a\nb");
  });
  assert_throws_js(TypeError, () => {
    headers.set("Set-Cookie", "a\rb");
  });
}, "Adding invalid Set-Cookie headers throws");

test(function () {
  const response = new Response();
  response.headers.append("Set-Cookie", "foo=bar");
  assert_array_equals(response.headers.getSetCookie(), []);
  response.headers.append("sEt-cOokIe", "bar=baz");
  assert_array_equals(response.headers.getSetCookie(), []);
}, "Set-Cookie is a forbidden response header");
