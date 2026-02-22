var RESOURCES_DIR = "../resources/";

function dirname(path) {
    return path.replace(/\/[^\/]*$/, '/')
}

function checkRequest(request, ExpectedValuesDict) {
  for (var attribute in ExpectedValuesDict) {
    switch(attribute) {
      case "headers":
        for (var key in ExpectedValuesDict["headers"].keys()) {
          assert_equals(request["headers"].get(key), ExpectedValuesDict["headers"].get(key),
            "Check headers attribute has " + key + ":" + ExpectedValuesDict["headers"].get(key));
        }
        break;

      case "body":
        //for checking body's content, a dedicated asyncronous/promise test should be used
        assert_true(request["headers"].has("Content-Type") , "Check request has body using Content-Type header")
        break;

      case "method":
      case "referrer":
      case "referrerPolicy":
      case "credentials":
      case "cache":
      case "redirect":
      case "integrity":
      case "url":
      case "destination":
        assert_equals(request[attribute], ExpectedValuesDict[attribute], "Check " + attribute + " attribute")
        break;

      default:
        break;
    }
  }
}

function stringToArray(str) {
  var array = new Uint8Array(str.length);
  for (var i=0, strLen = str.length; i < strLen; i++)
    array[i] = str.charCodeAt(i);
  return array;
}

function encode_utf8(str)
{
    if (self.TextEncoder)
        return (new TextEncoder).encode(str);
    return stringToArray(unescape(encodeURIComponent(str)));
}

function validateBufferFromString(buffer, expectedValue, message)
{
  return assert_array_equals(new Uint8Array(buffer !== undefined ? buffer : []), stringToArray(expectedValue), message);
}

function validateStreamFromString(reader, expectedValue, retrievedArrayBuffer) {
  // Passing Uint8Array for byte streams; non-byte streams will simply ignore it
  return reader.read(new Uint8Array(64)).then(function(data) {
    if (!data.done) {
      assert_true(data.value instanceof Uint8Array, "Fetch ReadableStream chunks should be Uint8Array");
      var newBuffer;
      if (retrievedArrayBuffer) {
        newBuffer =  new Uint8Array(data.value.length + retrievedArrayBuffer.length);
        newBuffer.set(retrievedArrayBuffer, 0);
        newBuffer.set(data.value, retrievedArrayBuffer.length);
      } else {
        newBuffer = data.value;
      }
      return validateStreamFromString(reader, expectedValue, newBuffer);
    }
    validateBufferFromString(retrievedArrayBuffer, expectedValue, "Retrieve and verify stream");
  });
}

function validateStreamFromPartialString(reader, expectedValue, retrievedArrayBuffer) {
  // Passing Uint8Array for byte streams; non-byte streams will simply ignore it
  return reader.read(new Uint8Array(64)).then(function(data) {
    if (!data.done) {
      assert_true(data.value instanceof Uint8Array, "Fetch ReadableStream chunks should be Uint8Array");
      var newBuffer;
      if (retrievedArrayBuffer) {
        newBuffer =  new Uint8Array(data.value.length + retrievedArrayBuffer.length);
        newBuffer.set(retrievedArrayBuffer, 0);
        newBuffer.set(data.value, retrievedArrayBuffer.length);
      } else {
        newBuffer = data.value;
      }
      return validateStreamFromPartialString(reader, expectedValue, newBuffer);
    }

    var string = new TextDecoder("utf-8").decode(retrievedArrayBuffer);
    return assert_true(string.search(expectedValue) != -1, "Retrieve and verify stream");
  });
}

// From streams tests
function delay(milliseconds)
{
  return new Promise(function(resolve) {
    step_timeout(resolve, milliseconds);
  });
}

function requestForbiddenHeaders(desc, forbiddenHeaders) {
  var url = RESOURCES_DIR + "inspect-headers.py";
  var requestInit = {"headers": forbiddenHeaders}
  var urlParameters = "?headers=" + Object.keys(forbiddenHeaders).join("|");

  promise_test(function(test){
    return fetch(url + urlParameters, requestInit).then(function(resp) {
      assert_equals(resp.status, 200, "HTTP status is 200");
      assert_equals(resp.type , "basic", "Response's type is basic");
      for (var header in forbiddenHeaders)
        assert_not_equals(resp.headers.get("x-request-" + header), forbiddenHeaders[header], header + " does not have the value we defined");
    });
  }, desc);
}
