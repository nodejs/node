addEventListener('fetch', evt => {
  if (evt.request.url.endsWith('slow')) {
    // Performance.now() might be a bit better here, but Date.now() has
    // better compat in workers right now.
    let start = Date.now();
    while(Date.now() - start < 2000);
  }
});
