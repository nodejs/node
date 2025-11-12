// META: timeout=long
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
  URL.revokeObjectURL(url);

  frame.onload = t.step_func_done(() => {
    assert_equals(frame.contentWindow.test_result, run_result);
  });
}, 'Fetching a blob URL immediately before revoking it works in an iframe.');

async_test(t => {
  const run_result = 'test_frame_OK';
  const blob_contents = '<!doctype html>\n<meta charset="utf-8">\n' +
    '<script>window.test_result = "' + run_result + '";</script>';
  const blob = new Blob([blob_contents], {type: 'text/html'});
  const url = URL.createObjectURL(blob);

  const frame = document.createElement('iframe');
  frame.setAttribute('src', '/common/blank.html');
  frame.setAttribute('style', 'display:none;');
  document.body.appendChild(frame);

  frame.onload = t.step_func(() => {
    frame.contentWindow.location = url;
    URL.revokeObjectURL(url);
    frame.onload = t.step_func_done(() => {
      assert_equals(frame.contentWindow.test_result, run_result);
    });
  });
}, 'Fetching a blob URL immediately before revoking it works in an iframe navigation.');

async_test(t => {
  const run_result = 'test_frame_OK';
  const blob_contents = '<!doctype html>\n<meta charset="utf-8">\n' +
    '<script>window.test_result = "' + run_result + '";</script>';
  const blob = new Blob([blob_contents], {type: 'text/html'});
  const url = URL.createObjectURL(blob);
  const win = window.open(url);
  URL.revokeObjectURL(url);
  add_completion_callback(() => { win.close(); });

  win.onload = t.step_func_done(() => {
    assert_equals(win.test_result, run_result);
  });
}, 'Opening a blob URL in a new window immediately before revoking it works.');

function receive_message_on_channel(t, channel_name) {
  const channel = new BroadcastChannel(channel_name);
  return new Promise(resolve => {
    channel.addEventListener('message', t.step_func(e => {
      resolve(e.data);
    }));
  });
}

function window_contents_for_channel(channel_name) {
  return '<!doctype html>\n' +
    '<script>\n' +
    'new BroadcastChannel("' + channel_name + '").postMessage("foobar");\n' +
    'self.close();\n' +
    '</script>';
}

async_test(t => {
  const channel_name = 'noopener-window-test';
  const blob = new Blob([window_contents_for_channel(channel_name)], {type: 'text/html'});
  receive_message_on_channel(t, channel_name).then(t.step_func_done(t => {
    assert_equals(t, 'foobar');
  }));
  const url = URL.createObjectURL(blob);
  const win = window.open();
  win.opener = null;
  win.location = url;
  URL.revokeObjectURL(url);
}, 'Opening a blob URL in a noopener about:blank window immediately before revoking it works.');

async_test(t => {
  const run_result = 'test_script_OK';
  const blob_contents = 'window.script_test_result = "' + run_result + '";';
  const blob = new Blob([blob_contents]);
  const url = URL.createObjectURL(blob);

  const e = document.createElement('script');
  e.setAttribute('src', url);
  e.onload = t.step_func_done(() => {
    assert_equals(window.script_test_result, run_result);
  });

  document.body.appendChild(e);
  URL.revokeObjectURL(url);
}, 'Fetching a blob URL immediately before revoking it works in <script> tags.');

async_test(t => {
  const channel_name = 'a-click-test';
  const blob = new Blob([window_contents_for_channel(channel_name)], {type: 'text/html'});
  receive_message_on_channel(t, channel_name).then(t.step_func_done(t => {
    assert_equals(t, 'foobar');
  }));
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.target = '_blank';
  document.body.appendChild(anchor);
  anchor.click();
  URL.revokeObjectURL(url);
}, 'Opening a blob URL in a new window by clicking an <a> tag works immediately before revoking the URL.');
