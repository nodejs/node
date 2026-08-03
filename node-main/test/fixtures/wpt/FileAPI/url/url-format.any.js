// META: timeout=long
const blob = new Blob(['test']);
const file = new File(['test'], 'name');

test(t => {
  const url_count = 5000;
  let list = [];

  t.add_cleanup(() => {
    for (let url of list) {
      URL.revokeObjectURL(url);
    }
  });

  for (let i = 0; i < url_count; ++i)
    list.push(URL.createObjectURL(blob));

  list.sort();

  for (let i = 1; i < list.length; ++i)
    assert_not_equals(list[i], list[i-1], 'generated Blob URLs should be unique');
}, 'Generated Blob URLs are unique');

test(() => {
  const url = URL.createObjectURL(blob);
  assert_equals(typeof url, 'string');
  assert_true(url.startsWith('blob:'));
}, 'Blob URL starts with "blob:"');

test(() => {
  const url = URL.createObjectURL(file);
  assert_equals(typeof url, 'string');
  assert_true(url.startsWith('blob:'));
}, 'Blob URL starts with "blob:" for Files');

test(() => {
  const url = URL.createObjectURL(blob);
  assert_equals(new URL(url).origin, location.origin);
  if (location.origin !== 'null') {
    assert_true(url.includes(location.origin));
    assert_true(url.startsWith('blob:' + location.protocol));
  }
}, 'Origin of Blob URL matches our origin');

test(() => {
  const url = URL.createObjectURL(blob);
  const url_record = new URL(url);
  assert_equals(url_record.protocol, 'blob:');
  assert_equals(url_record.origin, location.origin);
  assert_equals(url_record.host, '', 'host should be an empty string');
  assert_equals(url_record.port, '', 'port should be an empty string');
  const uuid_path_re = /\/[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
  assert_true(uuid_path_re.test(url_record.pathname), 'Path must end with a valid UUID');
  if (location.origin !== 'null') {
    const nested_url = new URL(url_record.pathname);
    assert_equals(nested_url.origin, location.origin);
    assert_equals(nested_url.pathname.search(uuid_path_re), 0, 'Path must be a valid UUID');
    assert_true(url.includes(location.origin));
    assert_true(url.startsWith('blob:' + location.protocol));
  }
}, 'Blob URL parses correctly');

test(() => {
  const url = URL.createObjectURL(file);
  assert_equals(new URL(url).origin, location.origin);
  if (location.origin !== 'null') {
    assert_true(url.includes(location.origin));
    assert_true(url.startsWith('blob:' + location.protocol));
  }
}, 'Origin of Blob URL matches our origin for Files');
