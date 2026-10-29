'use strict';

onfetch = function(e) {
  var headers = {};
  for (var header of e.request.headers) {
    var key = header[0], value = header[1];
    headers[key] = value;
  }
  var append_header_error = '';
  try {
    e.request.headers.append('Test-Header', 'TestValue');
  } catch (error) {
    append_header_error = error.name;
  }

  var request_construct_error = '';
  try {
    new Request(e.request, {method: 'GET'});
  } catch (error) {
    request_construct_error = error.name;
  }

  e.respondWith(new Response(JSON.stringify({
    url: e.request.url,
    method: e.request.method,
    referrer: e.request.referrer,
    headers: headers,
    mode: e.request.mode,
    credentials: e.request.credentials,
    redirect: e.request.redirect,
    append_header_error: append_header_error,
    request_construct_error: request_construct_error
  })));
};
