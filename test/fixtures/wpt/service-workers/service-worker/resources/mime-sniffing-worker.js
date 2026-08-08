self.addEventListener('fetch', function(event) {
    // Use an empty content-type value to force mime-sniffing.  Note, this
    // must be passed to the constructor since the mime-type of the Response
    // is fixed and cannot be later changed.
    var res = new Response('<!DOCTYPE html>\n<h1 id=\'testid\'>test</h1>', {
      headers: { 'content-type': '' }
    });
    event.respondWith(res);
  });
