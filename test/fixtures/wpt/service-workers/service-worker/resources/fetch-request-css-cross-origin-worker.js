importScripts('/common/get-host-info.sub.js');
importScripts('test-helpers.sub.js');

const HOST_INFO = get_host_info();
const REMOTE_ORIGIN = HOST_INFO.HTTPS_REMOTE_ORIGIN;
const BASE_PATH = base_path();
const CSS_FILE = 'fetch-request-css-cross-origin-mime-check-cross.css';
const HTML_FILE = 'fetch-request-css-cross-origin-mime-check-cross.html';

function add_pipe_header(url_str, header) {
  if (url_str.indexOf('?pipe=') == -1) {
    url_str += '?pipe=';
  } else {
    url_str += '|';
  }
  url_str += `header${header}`;
  return url_str;
}

self.addEventListener('fetch', function(event) {
    const url = new URL(event.request.url);

    const use_mime =
        (url.searchParams.get('mime') != 'no');
    const mime_header = '(Content-Type, text/css)';

    const use_cors =
        (url.searchParams.has('cors'));
    const cors_header = '(Access-Control-Allow-Origin, *)';

    const file = url.pathname.substring(url.pathname.lastIndexOf('/') + 1);

    // Respond with a cross-origin CSS resource, using CORS if desired.
    if (file == 'cross-origin-css.css') {
      let fetch_url =  REMOTE_ORIGIN + BASE_PATH + CSS_FILE;
      if (use_mime)
        fetch_url = add_pipe_header(fetch_url, mime_header);
      if (use_cors)
        fetch_url = add_pipe_header(fetch_url, cors_header);
      const mode = use_cors ? 'cors' : 'no-cors';
      event.respondWith(fetch(fetch_url, {'mode': mode}));
      return;
    }

    // Respond with a cross-origin CSS resource with an HTML name. This is only
    // used in the MIME sniffing test, so MIME is never added.
    if (file == 'cross-origin-html.css') {
      const fetch_url = REMOTE_ORIGIN + BASE_PATH + HTML_FILE;
      event.respondWith(fetch(fetch_url, {mode: 'no-cors'}));
      return;
    }

    // Respond with synthetic CSS.
    if (file == 'synthetic.css') {
      let headers = {};
      if (use_mime) {
        headers['Content-Type'] = 'text/css';
      }

      event.respondWith(new Response("#synthetic { color: blue; }", {headers}));
      return;
    }

    // Otherwise, fallback to network.
  });
