importScripts("worker-testharness.js");
importScripts("test-helpers.sub.js");
importScripts("/common/get-host-info.sub.js")
importScripts("testharness-helpers.js")

setup({ explicit_done: true });

self.onfetch = function(e) {
  if (e.request.url.indexOf("client-navigate-frame.html") >= 0) {
    return;
  }
  e.respondWith(new Response(e.clientId));
};

function pass(test, url) {
  return { result: test,
           url: url };
}

function fail(test, reason) {
  return { result: "FAILED " + test + " " + reason }
}

self.onmessage = function(e) {
  var port = e.data.port;
  var test = e.data.test;
  var clientId = e.data.clientId;
  var clientUrl = "";
  if (test === "test_client_navigate_success") {
    promise_test(function(t) {
      this.add_cleanup(() => port.postMessage(pass(test, clientUrl)));
      return self.clients.get(clientId)
                 .then(client => client.navigate("client-navigated-frame.html"))
                 .then(client => {
                   clientUrl = client.url;
                   assert_true(client instanceof WindowClient);
                 })
                 .catch(unreached_rejection(t));
    }, "Return value should be instance of WindowClient");
    done();
  } else if (test === "test_client_navigate_cross_origin") {
    promise_test(function(t) {
      this.add_cleanup(() => port.postMessage(pass(test, clientUrl)));
      var path = new URL('client-navigated-frame.html', self.location.href).pathname;
      var url = get_host_info()['HTTPS_REMOTE_ORIGIN'] + path;
      return self.clients.get(clientId)
                 .then(client => client.navigate(url))
                 .then(client => {
                   clientUrl = (client && client.url) || "";
                   assert_equals(client, null,
                                 'cross-origin navigate resolves with null');
                 })
                 .catch(unreached_rejection(t));
    }, "Navigating to different origin should resolve with null");
    done();
  } else if (test === "test_client_navigate_about_blank") {
    promise_test(function(t) {
      this.add_cleanup(function() { port.postMessage(pass(test, "")); });
      return self.clients.get(clientId)
                 .then(client => promise_rejects_js(t, TypeError, client.navigate("about:blank")))
                 .catch(unreached_rejection(t));
    }, "Navigating to about:blank should reject with TypeError");
    done();
  } else if (test === "test_client_navigate_mixed_content") {
    promise_test(function(t) {
      this.add_cleanup(function() { port.postMessage(pass(test, "")); });
      var path = new URL('client-navigated-frame.html', self.location.href).pathname;
      // Insecure URL should fail since the frame is owned by a secure parent
      // and navigating to http:// would create a mixed-content violation.
      var url = get_host_info()['HTTP_REMOTE_ORIGIN'] + path;
      return self.clients.get(clientId)
                 .then(client => promise_rejects_js(t, TypeError, client.navigate(url)))
                 .catch(unreached_rejection(t));
    }, "Navigating to mixed-content iframe should reject with TypeError");
    done();
  } else if (test === "test_client_navigate_redirect") {
    var host_info = get_host_info();
    var url = new URL(host_info['HTTPS_REMOTE_ORIGIN']).toString() +
              new URL("client-navigated-frame.html", location).pathname.substring(1);
    promise_test(function(t) {
      this.add_cleanup(() => port.postMessage(pass(test, clientUrl)));
      return self.clients.get(clientId)
                 .then(client => client.navigate("redirect.py?Redirect=" + url))
                 .then(client => {
                   clientUrl = (client && client.url) || ""
                   assert_equals(client, null);
                 })
                 .catch(unreached_rejection(t));
    }, "Redirecting to another origin should resolve with null");
    done();
  }
};
