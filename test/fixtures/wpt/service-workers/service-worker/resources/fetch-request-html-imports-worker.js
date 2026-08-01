importScripts('/common/get-host-info.sub.js');
var host_info = get_host_info();

self.addEventListener('fetch', function(event) {
    var url = event.request.url;
    if (url.indexOf('sample-dir') == -1) {
      return;
    }
    var result = 'mode=' + event.request.mode +
      ' credentials=' + event.request.credentials;
    if (url == host_info.HTTPS_ORIGIN + '/sample-dir/same.html') {
      event.respondWith(new Response(
        result +
        '<link id="same-same" rel="import" ' +
        'href="' + host_info.HTTPS_ORIGIN + '/sample-dir/same-same.html">' +
        '<link id="same-other" rel="import" ' +
        ' href="' + host_info.HTTPS_REMOTE_ORIGIN +
        '/sample-dir/same-other.html">'));
    } else if (url == host_info.HTTPS_REMOTE_ORIGIN + '/sample-dir/other.html') {
      event.respondWith(new Response(
        result +
        '<link id="other-same" rel="import" ' +
        ' href="' + host_info.HTTPS_ORIGIN + '/sample-dir/other-same.html">' +
        '<link id="other-other" rel="import" ' +
        ' href="' + host_info.HTTPS_REMOTE_ORIGIN +
        '/sample-dir/other-other.html">'));
    } else {
      event.respondWith(new Response(result));
    }
  });
