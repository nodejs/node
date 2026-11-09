// META: title=Blob constructor: endings option
[
  'transparent',
  'native'
].forEach(value => test(t => {
  assert_class_string(new Blob([], {endings: value}), 'Blob',
                      `Constructor should allow "${value}" endings`);
}, `Valid "endings" value: ${JSON.stringify(value)}`));

[
  null,
  '',
  'invalidEnumValue',
  'Transparent',
  'NATIVE',
  0,
  {}
].forEach(value => test(t => {
  assert_throws_js(TypeError, () => new Blob([], {endings: value}),
                   'Blob constructor should throw');
}, `Invalid "endings" value: ${JSON.stringify(value)}`));

test(t => {
  const test_error = {name: 'test'};
  assert_throws_exactly(
    test_error,
    () => new Blob([], { get endings() { throw test_error; }}),
    'Blob constructor should propagate exceptions from "endings" property');
}, 'Exception propagation from options');

test(t => {
  let got = false;
  new Blob([], { get endings() { got = true; } });
  assert_true(got, 'The "endings" property was accessed during construction.');
}, 'The "endings" options property is used');