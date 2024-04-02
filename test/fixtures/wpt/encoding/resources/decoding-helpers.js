// Decode an URL encoded string, using XHR and data: URL. Returns a Promise.
function decode(label, url_encoded_string) {
  return new Promise((resolve, reject) => {
    const req = new XMLHttpRequest;
    req.open('GET', `data:text/plain,${url_encoded_string}`);
    req.overrideMimeType(`text/plain; charset="${label}"`);
    req.send();
    req.onload = () => resolve(req.responseText);
    req.onerror = () => reject(new Error(req.statusText));
  });
}

// Convert code units in a decoded string into: "U+0001/U+0002/...'
function to_code_units(string) {
  return string.split('')
    .map(unit => unit.charCodeAt(0))
    .map(code => 'U+' + ('0000' + code.toString(16).toUpperCase()).slice(-4))
    .join('/');
}

function decode_test(label,
                     url_encoded_input,
                     expected_code_units,
                     description) {
  promise_test(() => {
    return decode(label, url_encoded_input)
      .then(decoded => to_code_units(decoded))
      .then(actual => {
        assert_equals(actual, expected_code_units, `Decoding with ${label}`);
      });
  }, description);
}
