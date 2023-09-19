const custom_cors_response = (payload, base_url) => {
  base_url = base_url || new URL(location.href);

  // Clone the given `payload` so that, as we modify it, we won't be mutating
  // the caller's value in unexpected ways.
  payload = Object.assign({}, payload);
  payload.headers = payload.headers || {};
  // Note that, in order to have out-of-the-box support for tests that don't
  // call `setup({'allow_uncaught_exception': true})` we return a no-op JS
  // payload. This approach will avoid hitting syntax errors if the resource is
  // interpreted as script. Without this workaround, the SyntaxError would be
  // caught by the test harness and trigger a test failure.
  payload.content = payload.content || '/* custom-cors-response.js content */';
  payload.status_code = payload.status_code || 200;

  // Assume that we'll be doing a CORS-enabled fetch so we'll need to set ACAO.
  const acao = "Access-Control-Allow-Origin";
  if (!(acao in payload.headers)) {
    payload.headers[acao] = '*';
  }

  if (!("Content-Type" in payload.headers)) {
    payload.headers["Content-Type"] = "text/javascript";
  }

  let ret = new URL("/common/CustomCorsResponse.py", base_url);
  for (const key in payload) {
    ret.searchParams.append(key, JSON.stringify(payload[key]));
  }

  return ret;
};
