// META: script=../resources/utils.js

function fetchNoCors(url, isOpaqueFiltered) {
  var urlQuery = "?pipe=header(x-is-filtered,value)"
  promise_test(function(test) {
    if (isOpaqueFiltered)
      return fetch(url + urlQuery, {"mode": "no-cors"}).then(function(resp) {
        assert_equals(resp.status, 0, "Opaque filter: status is 0");
        assert_equals(resp.statusText, "", "Opaque filter: statusText is \"\"");
        assert_equals(resp.url, "", "Opaque filter: url is \"\"");
        assert_equals(resp.type , "opaque", "Opaque filter: response's type is opaque");
        assert_equals(resp.headers.get("x-is-filtered"), null, "Header x-is-filtered is filtered");
      });
    else
      return fetch(url + urlQuery, {"mode": "no-cors"}).then(function(resp) {
        assert_equals(resp.status, 200, "HTTP status is 200");
        assert_equals(resp.type , "basic", "Response's type is basic");
        assert_equals(resp.headers.get("x-is-filtered"), "value", "Header x-is-filtered is not filtered");
      });
  }, "Fetch "+ url + " with no-cors mode");
}

fetchNoCors(RESOURCES_DIR + "top.txt", false);
fetchNoCors("http://{{host}}:{{ports[http][0]}}/fetch/api/resources/top.txt", false);
fetchNoCors("https://{{host}}:{{ports[https][0]}}/fetch/api/resources/top.txt", true);
fetchNoCors("http://{{host}}:{{ports[http][1]}}/fetch/api/resources/top.txt", true);

done();

