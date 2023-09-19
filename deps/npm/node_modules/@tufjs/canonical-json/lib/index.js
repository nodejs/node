const COMMA = ',';
const COLON = ':';
const LEFT_SQUARE_BRACKET = '[';
const RIGHT_SQUARE_BRACKET = ']';
const LEFT_CURLY_BRACKET = '{';
const RIGHT_CURLY_BRACKET = '}';

// Recursively encodes the supplied object according to the canonical JSON form
// as specified at http://wiki.laptop.org/go/Canonical_JSON. It's a restricted
// dialect of JSON in which keys are lexically sorted, floats are not allowed,
// and only double quotes and backslashes are escaped.
function canonicalize(object) {
  const buffer = [];
  if (typeof object === 'string') {
    buffer.push(canonicalizeString(object));
  } else if (typeof object === 'boolean') {
    buffer.push(JSON.stringify(object));
  } else if (Number.isInteger(object)) {
    buffer.push(JSON.stringify(object));
  } else if (object === null) {
    buffer.push(JSON.stringify(object));
  } else if (Array.isArray(object)) {
    buffer.push(LEFT_SQUARE_BRACKET);
    let first = true;
    object.forEach((element) => {
      if (!first) {
        buffer.push(COMMA);
      }
      first = false;
      buffer.push(canonicalize(element));
    });
    buffer.push(RIGHT_SQUARE_BRACKET);
  } else if (typeof object === 'object') {
    buffer.push(LEFT_CURLY_BRACKET);
    let first = true;
    Object.keys(object)
      .sort()
      .forEach((property) => {
        if (!first) {
          buffer.push(COMMA);
        }
        first = false;
        buffer.push(canonicalizeString(property));
        buffer.push(COLON);
        buffer.push(canonicalize(object[property]));
      });
    buffer.push(RIGHT_CURLY_BRACKET);
  } else {
    throw new TypeError('cannot encode ' + object.toString());
  }

  return buffer.join('');
}

// String canonicalization consists of escaping backslash (\) and double
// quote (") characters and wrapping the resulting string in double quotes.
function canonicalizeString(string) {
  const escapedString = string.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
  return '"' + escapedString + '"';
}

module.exports = {
  canonicalize,
};
