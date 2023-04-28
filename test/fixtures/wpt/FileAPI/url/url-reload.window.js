function blob_url_reload_test(t, revoke_before_reload) {
  const run_result = 'test_frame_OK';
  const blob_contents = '<!doctype html>\n<meta charset="utf-8">\n' +
    '<script>window.test_result = "' + run_result + '";</script>';
  const blob = new Blob([blob_contents], {type: 'text/html'});
  const url = URL.createObjectURL(blob);

  const frame = document.createElement('iframe');
  frame.setAttribute('src', url);
  frame.setAttribute('style', 'display:none;');
  document.body.appendChild(frame);

  frame.onload = t.step_func(() => {
    if (revoke_before_reload)
      URL.revokeObjectURL(url);
    assert_equals(frame.contentWindow.test_result, run_result);
    frame.contentWindow.test_result = null;
    frame.onload = t.step_func_done(() => {
      assert_equals(frame.contentWindow.test_result, run_result);
    });
    // Slight delay before reloading to ensure revoke actually has had a chance
    // to be processed.
    t.step_timeout(() => {
      frame.contentWindow.location.reload();
    }, 250);
  });
}

async_test(t => {
  blob_url_reload_test(t, false);
}, 'Reloading a blob URL succeeds.');


async_test(t => {
  blob_url_reload_test(t, true);
}, 'Reloading a blob URL succeeds even if the URL was revoked.');
