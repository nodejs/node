self.addEventListener('fetch', function(event) {
    var testcase = new URL(event.request.url).search;
    switch (testcase) {
    case '?response-object':
      event.respondWith(new Response('body'));
      break;
    case '?response-promise-object':
      event.respondWith(Promise.resolve(new Response('body')));
      break;
    case '?other-value':
      event.respondWith(new Object());
      break;
    }
  });
