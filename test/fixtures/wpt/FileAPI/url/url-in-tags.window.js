setup(() => {
  const viewport_meta = document.createElement('meta');
  viewport_meta.name = "viewport";
  viewport_meta.content = "width=device-width,initial-scale=1";
  document.head.appendChild(viewport_meta);
});

async_test(t => {
  const run_result = 'test_script_OK';
  const blob_contents = 'window.test_result = "' + run_result + '";';
  const blob = new Blob([blob_contents]);
  const url = URL.createObjectURL(blob);

  const e = document.createElement('script');
  e.setAttribute('src', url);
  e.onload = t.step_func_done(() => {
    assert_equals(window.test_result, run_result);
  });

  document.body.appendChild(e);
}, 'Blob URLs can be used in <script> tags');

async_test(t => {
  const run_result = 'test_frame_OK';
  const blob_contents = '<!doctype html>\n<meta charset="utf-8">\n' +
    '<script>window.test_result = "' + run_result + '";</script>';
  const blob = new Blob([blob_contents], {type: 'text/html'});
  const url = URL.createObjectURL(blob);

  const frame = document.createElement('iframe');
  frame.setAttribute('src', url);
  frame.setAttribute('style', 'display:none;');
  document.body.appendChild(frame);

  frame.onload = t.step_func_done(() => {
    assert_equals(frame.contentWindow.test_result, run_result);
  });
}, 'Blob URLs can be used in iframes, and are treated same origin');

async_test(t => {
  const blob_contents = '<!doctype html>\n<meta charset="utf-8">\n' +
    '<style>body { margin: 0; } .block { height: 5000px; }</style>\n' +
    '<body>\n' +
    '<a id="block1"></a><div class="block"></div>\n' +
    '<a id="block2"></a><div class="block"></div>';
  const blob = new Blob([blob_contents], {type: 'text/html'});
  const url = URL.createObjectURL(blob);

  const frame = document.createElement('iframe');
  frame.setAttribute('src', url + '#block2');
  document.body.appendChild(frame);
  frame.contentWindow.onscroll = t.step_func_done(() => {
    assert_equals(frame.contentWindow.scrollY, 5000);
  });
}, 'Blob URL fragment is implemented.');
