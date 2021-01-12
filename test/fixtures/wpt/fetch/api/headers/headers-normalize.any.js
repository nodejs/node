// META: title=Headers normalize values
// META: global=window,worker

var headerDictWS = {"name1": " space ",
                    "name2": "\ttab\t",
                    "name3": " spaceAndTab\t",
                    "name4": "\r\n newLine", //obs-fold cases
                    "name5": "newLine\r\n ",
                    "name6": "\r\n\tnewLine",
                    };

test(function() {
  var headers = new Headers(headerDictWS);
  for (name in headerDictWS)
    assert_equals(headers.get(name), headerDictWS[name].trim(),
      "name: " + name + " has normalized value: " + headerDictWS[name].trim());
}, "Create headers with not normalized values");

test(function() {
  var headers = new Headers();
  for (name in headerDictWS) {
    headers.append(name, headerDictWS[name]);
    assert_equals(headers.get(name), headerDictWS[name].trim(),
      "name: " + name + " has value: " + headerDictWS[name].trim());
  }
}, "Check append method with not normalized values");

test(function() {
  var headers = new Headers();
  for (name in headerDictWS) {
    headers.set(name, headerDictWS[name]);
    assert_equals(headers.get(name), headerDictWS[name].trim(),
      "name: " + name + " has value: " + headerDictWS[name].trim());
  }
}, "Check set method with not normalized values");
