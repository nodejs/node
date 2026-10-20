importScripts('/common/get-host-info.sub.js');

const response_text = 'This load was successfully intercepted.';
const response_script =
    `const message = 'This load was successfully intercepted.';`;

self.onfetch = event => {
  const url = event.request.url;
  if (url.indexOf('synthesized-response.txt') != -1) {
    event.respondWith(new Response(response_text));
  } else if (url.indexOf('synthesized-response.js') != -1) {
    event.respondWith(new Response(
        response_script,
        {headers: {'Content-Type': 'application/javascript'}}));
  }
};
