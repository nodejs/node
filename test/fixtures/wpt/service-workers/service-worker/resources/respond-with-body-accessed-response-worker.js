importScripts('/common/get-host-info.sub.js');
importScripts('test-helpers.sub.js');

function getQueryParams(url) {
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

function createResponse(params) {
  if (params['type'] == 'basic') {
    return fetch('respond-with-body-accessed-response.jsonp');
  }
  if (params['type'] == 'opaque') {
    return fetch(get_host_info()['HTTPS_REMOTE_ORIGIN'] + base_path() +
          'respond-with-body-accessed-response.jsonp',
          {mode: 'no-cors'});
  }
  if (params['type'] == 'default') {
    return Promise.resolve(new Response('callback(\'OK\');'));
  }

  return Promise.reject(new Error('unexpected type :' + params['type']));
}

function cloneResponseIfNeeded(params, response) {
  if (params['clone'] == '1') {
    return response.clone();
  } else if (params['clone'] == '2') {
    response.clone();
    return response;
  }
  return response;
}

function passThroughCacheIfNeeded(params, request, response) {
  return new Promise(function(resolve) {
      if (params['passThroughCache'] == 'true') {
        var cache_name = request.url;
        var cache;
        self.caches.delete(cache_name)
          .then(function() {
              return self.caches.open(cache_name);
            })
          .then(function(c) {
              cache = c;
              return cache.put(request, response);
            })
          .then(function() {
              return cache.match(request.url);
            })
          .then(function(res) {
              // Touch .body here to test the behavior after touching it.
              res.body;
              resolve(res);
            });
      } else {
        resolve(response);
      }
    })
}

self.addEventListener('fetch', function(event) {
    if (event.request.url.indexOf('TestRequest') == -1) {
      return;
    }
    var params = getQueryParams(event.request.url);
    event.respondWith(
        createResponse(params)
          .then(function(response) {
              // Touch .body here to test the behavior after touching it.
              response.body;
              return cloneResponseIfNeeded(params, response);
            })
          .then(function(response) {
              // Touch .body here to test the behavior after touching it.
              response.body;
              return passThroughCacheIfNeeded(params, event.request, response);
            })
          .then(function(response) {
              // Touch .body here to test the behavior after touching it.
              response.body;
              return response;
            }));
  });
