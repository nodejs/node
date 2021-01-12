function headerNames(headers) {
  let names = [];
  for (let header of headers) {
    names.push(header[0].toLowerCase());
  }
  return names;
}

/*
  Check preflight is done
  Control if server allows method and headers and check accordingly
  Check control access headers added by UA (for method and headers)
*/
function corsPreflight(desc, corsUrl, method, allowed, headers, safeHeaders) {
  return promise_test(function(test) {
    var uuid_token = token();
    return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token).then(function(response) {
      var url = corsUrl + (corsUrl.indexOf("?") === -1 ? "?" : "&");
      var urlParameters = "token=" + uuid_token + "&max_age=0";
      var requestInit = {"mode": "cors", "method": method};
      var requestHeaders = [];
      if (headers)
        requestHeaders.push.apply(requestHeaders, headers);
      if (safeHeaders)
        requestHeaders.push.apply(requestHeaders, safeHeaders);
      requestInit["headers"] = requestHeaders;

      if (allowed) {
        urlParameters += "&allow_methods=" + method + "&control_request_headers";
        if (headers) {
          //Make the server allow the headers
          urlParameters += "&allow_headers=" + headerNames(headers).join("%20%2C");
        }
        return fetch(url + urlParameters, requestInit).then(function(resp) {
          assert_equals(resp.status, 200, "Response's status is 200");
          assert_equals(resp.headers.get("x-did-preflight"), "1", "Preflight request has been made");
          if (headers) {
            var actualHeaders = resp.headers.get("x-control-request-headers").toLowerCase().split(",");
            for (var i in actualHeaders)
              actualHeaders[i] = actualHeaders[i].trim();
            for (var header of headers)
              assert_in_array(header[0].toLowerCase(), actualHeaders, "Preflight asked permission for header: " + header);

            let accessControlAllowHeaders = headerNames(headers).sort().join(",");
            assert_equals(resp.headers.get("x-control-request-headers"), accessControlAllowHeaders, "Access-Control-Allow-Headers value");
            return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token);
          } else {
            assert_equals(resp.headers.get("x-control-request-headers"), null, "Access-Control-Request-Headers should be omitted")
          }
        });
      } else {
        return promise_rejects_js(test, TypeError, fetch(url + urlParameters, requestInit)).then(function(){
          return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token);
        });
      }
    });
  }, desc);
}
