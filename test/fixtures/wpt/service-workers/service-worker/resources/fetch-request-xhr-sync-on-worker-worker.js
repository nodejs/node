'use strict';

self.onfetch = function(event) {
  if (event.request.url.indexOf('non-existent-file.txt') !== -1) {
    event.respondWith(new Response('Response from service worker'));
  } else if (event.request.url.indexOf('/iframe_page') !== -1) {
    event.respondWith(new Response(
        '<!DOCTYPE html>\n' +
        '<script>\n' +
        'function performSyncXHROnWorker(url) {\n' +
        '  return new Promise((resolve) => {\n' +
        '    var worker =\n' +
        '        new Worker(\'./worker_script\');\n' +
        '    worker.addEventListener(\'message\', (msg) => {\n' +
        '      resolve(msg.data);\n' +
        '    });\n' +
        '    worker.postMessage({\n' +
        '      url: url\n' +
        '    });\n' +
        '  });\n' +
        '}\n' +
        '</script>',
        {
          headers: [['content-type', 'text/html']]
        }));
  } else if (event.request.url.indexOf('/worker_script') !== -1) {
    event.respondWith(new Response(
        'self.onmessage = (msg) => {' +
        '  const syncXhr = new XMLHttpRequest();' +
        '  syncXhr.open(\'GET\', msg.data.url, false);' +
        '  syncXhr.send();' +
        '  self.postMessage({' +
        '    status: syncXhr.status,' +
        '    responseText: syncXhr.responseText' +
        '  });' +
        '}',
        {
          headers: [['content-type', 'application/javascript']]
        }));
  }
};
