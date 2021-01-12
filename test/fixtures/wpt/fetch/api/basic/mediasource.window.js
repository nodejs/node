promise_test(t => {
  const mediaSource = new MediaSource(),
        mediaSourceURL = URL.createObjectURL(mediaSource);
  return promise_rejects_js(t, TypeError, fetch(mediaSourceURL));
}, "Cannot fetch blob: URL from a MediaSource");
