// Verify that calling Response clone() in a detached iframe doesn't crash.
// Regression test for https://crbug.com/1082688.

'use strict';

promise_test(async () => {
  // Wait for the document body to be available.
  await new Promise(resolve => {
    onload = resolve;
  });

  window.iframe = document.createElement('iframe');
  document.body.appendChild(iframe);
  iframe.srcdoc = `<!doctype html>
<script>
const response = new Response('body');
window.parent.postMessage('okay', '*');
window.parent.iframe.remove();
response.clone();
</script>
`;

  await new Promise(resolve => {
    onmessage = evt => {
      if (evt.data === 'okay') {
        resolve();
      }
    };
  });

  // If it got here without crashing, the test passed.
}, 'clone within removed iframe should not crash');
