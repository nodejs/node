async_test(t => {
  // This could be detected as ISO-2022-JP, in which case there would be no
  // <textarea>, and thus the script inside would be interpreted as actual
  // script.
  const blob = new Blob(
      [
        `aaa\u001B$@<textarea>\u001B(B<script>/* xss */<\/script></textarea>bbb`
      ],
      {type: 'text/html;charset=utf-8'});
  const url = URL.createObjectURL(blob);
  const win = window.open(url);
  t.add_cleanup(() => {
    win.close();
  });

  win.onload = t.step_func_done(() => {
    assert_equals(win.document.charset, 'UTF-8');
  });
}, 'Blob charset should override any auto-detected charset.');

async_test(t => {
  const blob = new Blob(
      [`<!doctype html>\n<meta charset="ISO-8859-1">`],
      {type: 'text/html;charset=utf-8'});
  const url = URL.createObjectURL(blob);
  const win = window.open(url);
  t.add_cleanup(() => {
    win.close();
  });

  win.onload = t.step_func_done(() => {
    assert_equals(win.document.charset, 'UTF-8');
  });
}, 'Blob charset should override <meta charset>.');
