// META: global=window,worker
// META: title=Response init: simple cases

var defaultValues = { "type" : "default",
                      "url" : "",
                      "ok" : true,
                      "status" : 200,
                      "statusText" : "",
                      "body" : null
};

var statusCodes = { "givenValues" : [200, 300, 400, 500, 599],
                    "expectedValues" : [200, 300, 400, 500, 599]
};
var statusTexts = { "givenValues" : ["", "OK", "with space", String.fromCharCode(0x80)],
                    "expectedValues" : ["", "OK", "with space", String.fromCharCode(0x80)]
};
var initValuesDict = { "status" : statusCodes,
                        "statusText" : statusTexts
};

function isOkStatus(status) {
  return 200 <= status &&  299 >= status;
}

var response = new Response();
for (var attributeName in defaultValues) {
  test(function() {
    var expectedValue = defaultValues[attributeName];
    assert_equals(response[attributeName], expectedValue,
      "Expect default response." + attributeName + " is " + expectedValue);
  }, "Check default value for " + attributeName + " attribute");
}

for (var attributeName in initValuesDict) {
  test(function() {
    var valuesToTest = initValuesDict[attributeName];
    for (var valueIdx in valuesToTest["givenValues"]) {
      var givenValue = valuesToTest["givenValues"][valueIdx];
      var expectedValue = valuesToTest["expectedValues"][valueIdx];
      var responseInit = {};
      responseInit[attributeName] = givenValue;
      var response = new Response("", responseInit);
      assert_equals(response[attributeName], expectedValue,
        "Expect response." + attributeName + " is " + expectedValue +
        " when initialized with " + givenValue);
      assert_equals(response.ok, isOkStatus(response.status),
        "Expect response.ok is " + isOkStatus(response.status));
    }
  }, "Check " + attributeName + " init values and associated getter");
}

test(function() {
  const response1 = new Response("");
  assert_equals(response1.headers, response1.headers);

  const response2 = new Response("", {"headers": {"X-Foo": "bar"}});
  assert_equals(response2.headers, response2.headers);
  const headers = response2.headers;
  response2.headers.set("X-Foo", "quux");
  assert_equals(headers, response2.headers);
  headers.set("X-Other-Header", "baz");
  assert_equals(headers, response2.headers);
}, "Test that Response.headers has the [SameObject] extended attribute");
