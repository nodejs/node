// This is a regression test for Chromium issue https://crbug.com/1427266.
test(() => {
  const iframe = document.createElement('iframe');
  document.body.append(iframe);
  const otherRequest = iframe.contentWindow.Request;
  iframe.remove();
  const r1 = new otherRequest('resource', { method: 'POST', body: 'string' });
  const r2 = new otherRequest(r1);
  assert_true(r1.bodyUsed);
  assert_false(r2.bodyUsed);
}, 'creating a request from another request in a detached realm should work');
