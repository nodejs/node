// By default, this worker responds to fetch events with
// respondWith(fetch(request)). Additionally, if the request has a &url
// parameter, it fetches the provided URL instead. Because it forwards fetch
// events to this other URL, it is called the "fetch rewrite" worker.
//
// The worker also looks for other params on the request to do more custom
// behavior, like falling back to network or throwing an error.

function get_query_params(url) {
  var search = (new URL(url)).search;
  if (!search) {
    return {};
  }
  var ret = {};
  var params = search.substring(1).split('&');
  params.forEach(function(param) {
      var element = param.split('=');
      ret[decodeURIComponent(element[0])] = decodeURIComponent(element[1]);
    });
  return ret;
}

function get_request_init(base, params) {
  var init = {};
  init['method'] = params['method'] || base['method'];
  init['mode'] = params['mode'] || base['mode'];
  if (init['mode'] == 'navigate') {
    init['mode'] = 'same-origin';
  }
  init['credentials'] = params['credentials'] || base['credentials'];
  init['redirect'] = params['redirect-mode'] || base['redirect'];
  return init;
}

self.addEventListener('fetch', function(event) {
    var params = get_query_params(event.request.url);
    var init = get_request_init(event.request, params);
    var url = params['url'];
    if (params['ignore']) {
      return;
    }
    if (params['throw']) {
      throw new Error('boom');
    }
    if (params['reject']) {
      event.respondWith(new Promise(function(resolve, reject) {
          reject();
        }));
      return;
    }
    if (params['resolve-null']) {
      event.respondWith(new Promise(function(resolve) {
          resolve(null);
        }));
      return;
    }
    if (params['generate-png']) {
      var binary = atob(
          'iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAA' +
          'RnQU1BAACxjwv8YQUAAAAJcEhZcwAADsQAAA7EAZUrDhsAAAAhSURBVDhPY3wro/Kf' +
          'gQLABKXJBqMGjBoAAqMGDLwBDAwAEsoCTFWunmQAAAAASUVORK5CYII=');
      var array = new Uint8Array(binary.length);
      for(var i = 0; i < binary.length; i++) {
        array[i] = binary.charCodeAt(i);
      };
      event.respondWith(new Response(new Blob([array], {type: 'image/png'})));
      return;
    }
    if (params['check-ua-header']) {
      var ua = event.request.headers.get('User-Agent');
      if (ua) {
        // We have a user agent!
        event.respondWith(new Response(new Blob([ua])));
      } else {
        // We don't have a user-agent!
        event.respondWith(new Response(new Blob(["NO_UA"])));
      }
      return;
    }
    if (params['check-accept-header']) {
      var accept = event.request.headers.get('Accept');
      if (accept) {
        event.respondWith(new Response(accept));
      } else {
        event.respondWith(new Response('NO_ACCEPT'));
      }
      return;
    }
    event.respondWith(new Promise(function(resolve, reject) {
        var request = event.request;
        if (url) {
          request = new Request(url, init);
        } else if (params['change-request']) {
          request = new Request(request, init);
        }
        const response_promise = params['navpreload'] ? event.preloadResponse
                                                      : fetch(request);
        response_promise.then(function(response) {
          var expectedType = params['expected_type'];
          if (expectedType && response.type !== expectedType) {
            // Resolve a JSON object with a failure instead of rejecting
            // in order to distinguish this from a NetworkError, which
            // may be expected even if the type is correct.
            resolve(new Response(JSON.stringify({
              result: 'failure',
              detail: 'got ' + response.type + ' Response.type instead of ' +
                      expectedType
            })));
          }

          var expectedRedirected = params['expected_redirected'];
          if (typeof expectedRedirected !== 'undefined') {
            var expected_redirected = (expectedRedirected === 'true');
            if(response.redirected !== expected_redirected) {
              // This is simply determining how to pass an error to the outer
              // test case(fetch-request-redirect.https.html).
              var execptedResolves = params['expected_resolves'];
              if (execptedResolves === 'true') {
                // Reject a JSON object with a failure since promise is expected
                // to be resolved.
                reject(new Response(JSON.stringify({
                  result: 'failure',
                  detail: 'got '+ response.redirected +
                          ' Response.redirected instead of ' +
                          expectedRedirected
                })));
              } else {
                // Resolve a JSON object with a failure since promise is
                // expected to be rejected.
                resolve(new Response(JSON.stringify({
                  result: 'failure',
                  detail: 'got '+ response.redirected +
                          ' Response.redirected instead of ' +
                          expectedRedirected
                })));
              }
            }
          }

          if (params['clone']) {
            response = response.clone();
          }

          // |cache| means to bounce responses through Cache Storage and back.
          if (params['cache']) {
            var cacheName = "cached-fetches-" + performance.now() + "-" +
                            event.request.url;
            var cache;
            var cachedResponse;
            return self.caches.open(cacheName).then(function(opened) {
              cache = opened;
              return cache.put(request, response);
            }).then(function() {
              return cache.match(request);
            }).then(function(cached) {
              cachedResponse = cached;
              return self.caches.delete(cacheName);
            }).then(function() {
               resolve(cachedResponse);
            });
          } else {
            resolve(response);
          }
        }, reject)
      }));
  });
