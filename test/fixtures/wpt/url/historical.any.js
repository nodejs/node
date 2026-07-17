if (self.location) {
  test(function() {
    assert_false("searchParams" in self.location,
                "location object should not have a searchParams attribute");
  }, "searchParams on location object");
}

if(self.GLOBAL.isWindow()) {
  test(() => {
    assert_false("searchParams" in document.createElement("a"))
    assert_false("searchParams" in document.createElement("area"))
  }, "<a> and <area>.searchParams should be undefined");
}

test(function() {
  var url = new URL("./foo", "http://www.example.org");
  assert_equals(url.href, "http://www.example.org/foo");
  assert_throws_js(TypeError, function() {
    url.href = "./bar";
  });
}, "Setting URL's href attribute and base URLs");

test(function() {
  assert_equals(URL.domainToASCII, undefined);
}, "URL.domainToASCII should be undefined");

test(function() {
  assert_equals(URL.domainToUnicode, undefined);
}, "URL.domainToUnicode should be undefined");

test(() => {
  assert_throws_dom("DataCloneError", () => self.structuredClone(new URL("about:blank")));
}, "URL: no structured serialize/deserialize support");

test(() => {
  assert_throws_dom("DataCloneError", () => self.structuredClone(new URLSearchParams()));
}, "URLSearchParams: no structured serialize/deserialize support");

test(() => {
  const url = new URL("about:blank");
  url.toString = () => { throw 1 };
  assert_throws_exactly(1, () => new URL(url), "url argument");
  assert_throws_exactly(1, () => new URL("about:blank", url), "base argument");
}, "Constructor only takes strings");

done();
