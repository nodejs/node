# Query string

<!--introduced_in=v0.1.25-->

> Stability: 2 - Stable

<!--name=querystring-->

<!-- source_link=lib/querystring.js -->

The `node:querystring` module provides utilities for parsing and formatting URL
query strings. It can be accessed using:

```js
const querystring = require('node:querystring');
```

`querystring` is more performant than {URLSearchParams} but is not a
standardized API. Use {URLSearchParams} when performance is not critical or
when compatibility with browser code is desirable.

## Common Use Cases

The `querystring` module is commonly used for:

* Parsing form data from HTTP POST requests
* Processing URL query parameters in web applications
* Building query strings for API requests
* Converting objects to URL-encoded strings for form submissions

```js
const querystring = require('node:querystring');
const http = require('node:http');

// Example: Parsing form data in an HTTP server
const server = http.createServer((req, res) => {
  if (req.method === 'POST') {
    let body = '';
    req.on('data', chunk => {
      body += chunk.toString();
    });
    req.on('end', () => {
      const formData = querystring.parse(body);
      console.log('Form data:', formData);
      res.end('Form received');
    });
  }
});
```

## `querystring.decode()`

<!-- YAML
added: v0.1.99
-->

The `querystring.decode()` function is an alias for `querystring.parse()`.

```js
const querystring = require('node:querystring');

// These are equivalent:
const parsed1 = querystring.decode('name=John&age=30');
const parsed2 = querystring.parse('name=John&age=30');
console.log(parsed1); // { name: 'John', age: '30' }
console.log(parsed2); // { name: 'John', age: '30' }
```

## `querystring.encode()`

<!-- YAML
added: v0.1.99
-->

The `querystring.encode()` function is an alias for `querystring.stringify()`.

```js
const querystring = require('node:querystring');

// These are equivalent:
const encoded1 = querystring.encode({ name: 'John', age: 30 });
const encoded2 = querystring.stringify({ name: 'John', age: 30 });
console.log(encoded1); // 'name=John&age=30'
console.log(encoded2); // 'name=John&age=30'
```

## `querystring.escape(str)`

<!-- YAML
added: v0.1.25
-->

* `str` {string}

The `querystring.escape()` method performs URL percent-encoding on the given
`str` in a manner that is optimized for the specific requirements of URL
query strings.

The `querystring.escape()` method is used by `querystring.stringify()` and is
generally not expected to be used directly. It is exported primarily to allow
application code to provide a replacement percent-encoding implementation if
necessary by assigning `querystring.escape` to an alternative function.

```js
const querystring = require('node:querystring');

// Basic usage
console.log(querystring.escape('hello world')); // 'hello%20world'
console.log(querystring.escape('user@example.com')); // 'user%40example.com'
console.log(querystring.escape('price=$100')); // 'price%3D%24100'

// Characters that get encoded
console.log(querystring.escape('a+b=c&d')); // 'a%2Bb%3Dc%26d'
```

## `querystring.parse(str[, sep[, eq[, options]]])`

<!-- YAML
added: v0.1.25
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/10967
    description: Multiple empty entries are now parsed correctly (e.g. `&=&=`).
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/6055
    description: The returned object no longer inherits from `Object.prototype`.
  - version:
    - v6.0.0
    - v4.2.4
    pr-url: https://github.com/nodejs/node/pull/3807
    description: The `eq` parameter may now have a length of more than `1`.
-->

* `str` {string} The URL query string to parse (without the leading `?`)
* `sep` {string} The substring used to delimit key and value pairs in the
  query string. **Default:** `'&'`. Common alternatives include `';'` for some APIs.
* `eq` {string}. The substring used to delimit keys and values in the
  query string. **Default:** `'='`. Can be customized for non-standard formats.
* `options` {Object}
  * `decodeURIComponent` {Function} The function to use when decoding
    percent-encoded characters in the query string. **Default:**
    `querystring.unescape()`. Useful for custom encoding schemes.
  * `maxKeys` {number} Specifies the maximum number of keys to parse.
    Specify `0` to remove key counting limitations. **Default:** `1000`.
    Prevents memory exhaustion from malicious input.

The `querystring.parse()` method parses a URL query string (`str`) into a
collection of key and value pairs.

### Basic Examples

```js
const querystring = require('node:querystring');

// Simple key-value pairs
const simple = querystring.parse('name=John&age=30&city=NewYork');
console.log(simple);
// Output: { name: 'John', age: '30', city: 'NewYork' }

// Multiple values for the same key become arrays
const multiple = querystring.parse('color=red&color=blue&color=green');
console.log(multiple);
// Output: { color: ['red', 'blue', 'green'] }

// URL-encoded characters are automatically decoded
const encoded = querystring.parse('message=Hello%20World&email=user%40example.com');
console.log(encoded);
// Output: { message: 'Hello World', email: 'user@example.com' }
```

### Advanced Usage with Custom Separators

```js
// Using semicolon as separator (common in some APIs)
const semicolon = querystring.parse('name=John;age=30;city=NewYork', ';');
console.log(semicolon);
// Output: { name: 'John', age: '30', city: 'NewYork' }

// Using custom key-value separator
const custom = querystring.parse('name:John;age:30', ';', ':');
console.log(custom);
// Output: { name: 'John', age: '30' }
```

The object returned by the `querystring.parse()` method _does not_
prototypically inherit from the JavaScript `Object`. This means that typical
`Object` methods such as `obj.toString()`, `obj.hasOwnProperty()`, and others
are not defined and _will not work_.

### Error Handling and Edge Cases

```js
const querystring = require('node:querystring');

// Empty values are handled gracefully
const empty = querystring.parse('name=&age=30&city=');
console.log(empty);
// Output: { name: '', age: '30', city: '' }

// Keys without values default to empty string
const noValues = querystring.parse('debug&verbose&name=John');
console.log(noValues);
// Output: { debug: '', verbose: '', name: 'John' }

// Malformed input is handled safely
const malformed = querystring.parse('name=John&&&age=30&=invalid');
console.log(malformed);
// Output: { name: 'John', age: '30', '': 'invalid' }

// Limiting the number of keys (security consideration)
const limited = querystring.parse('a=1&b=2&c=3&d=4', null, null, { maxKeys: 2 });
console.log(limited);
// Output: { a: '1', b: '2' } (only first 2 keys parsed)
```

### Custom Encoding Handling

By default, percent-encoded characters within the query string will be assumed
to use UTF-8 encoding. If an alternative character encoding is used, then an
alternative `decodeURIComponent` option will need to be specified:

```js
// Example with custom decoder for legacy encoding
function customDecoder(str) {
  try {
    return decodeURIComponent(str);
  } catch (err) {
    // Fallback for malformed sequences
    return str;
  }
}

const safeParsed = querystring.parse('name=John%20Doe&invalid=%ZZ', null, null, {
  decodeURIComponent: customDecoder
});
console.log(safeParsed);
// Output: { name: 'John Doe', invalid: '%ZZ' }
```

## `querystring.stringify(obj[, sep[, eq[, options]]])`

<!-- YAML
added: v0.1.25
-->

* `obj` {Object} The object to serialize into a URL query string. Only own enumerable properties are processed.
* `sep` {string} The substring used to delimit key and value pairs in the
  query string. **Default:** `'&'`. Use `';'` for APIs that expect semicolon separation.
* `eq` {string}. The substring used to delimit keys and values in the
  query string. **Default:** `'='`. Can be customized for specific API requirements.
* `options`
  * `encodeURIComponent` {Function} The function to use when converting
    URL-unsafe characters to percent-encoding in the query string. **Default:**
    `querystring.escape()`. Useful for custom encoding schemes.

The `querystring.stringify()` method produces a URL query string from a
given `obj` by iterating through the object's "own properties".

It serializes the following types of values passed in `obj`:
{string|number|bigint|boolean|string\[]|number\[]|bigint\[]|boolean\[]}
The numeric values must be finite. Any other input values will be coerced to
empty strings.

### Basic Examples

```js
const querystring = require('node:querystring');

// Simple object to query string
const basic = querystring.stringify({ name: 'John', age: 30, active: true });
console.log(basic);
// Output: 'name=John&age=30&active=true'

// Arrays become multiple entries with the same key
const withArrays = querystring.stringify({ 
  colors: ['red', 'blue', 'green'], 
  size: 'large' 
});
console.log(withArrays);
// Output: 'colors=red&colors=blue&colors=green&size=large'

// Empty values are preserved
const withEmpty = querystring.stringify({ name: 'John', comment: '', active: true });
console.log(withEmpty);
// Output: 'name=John&comment=&active=true'
```

### Custom Separators

```js
// Using semicolon separator (common for some APIs)
const semicolon = querystring.stringify({ name: 'John', age: 30 }, ';');
console.log(semicolon);
// Output: 'name=John;age=30'

// Custom key-value separator
const custom = querystring.stringify({ name: 'John', age: 30 }, ';', ':');
console.log(custom);
// Output: 'name:John;age:30'
```

### Data Type Handling

```js
// Different data types are converted to strings
const mixed = querystring.stringify({
  string: 'hello',
  number: 42,
  bigint: 123n,
  boolean: true,
  array: [1, 2, 3]
});
console.log(mixed);
// Output: 'string=hello&number=42&bigint=123&boolean=true&array=1&array=2&array=3'
```

### Error Handling and Edge Cases

```js
const querystring = require('node:querystring');

// Null and undefined values are converted to empty strings
const withNulls = querystring.stringify({ 
  name: 'John', 
  middle: null, 
  last: undefined,
  age: 30 
});
console.log(withNulls);
// Output: 'name=John&middle=&last=&age=30'

// Objects and functions are converted to empty strings
const withObjects = querystring.stringify({
  name: 'John',
  data: { nested: 'object' },
  fn: function() { return 'test'; },
  age: 30
});
console.log(withObjects);
// Output: 'name=John&data=&fn=&age=30'

// Non-finite numbers are converted to empty strings
const withInfinite = querystring.stringify({
  valid: 42,
  infinite: Infinity,
  notANumber: NaN,
  negative: -Infinity
});
console.log(withInfinite);
// Output: 'valid=42&infinite=&notANumber=&negative='
```

### URL Encoding and Special Characters

```js
// Special characters are automatically encoded
const special = querystring.stringify({
  message: 'Hello World!',
  email: 'user@example.com',
  symbols: 'a+b=c&d',
  unicode: '中文测试'
});
console.log(special);
// Output: 'message=Hello%20World!&email=user%40example.com&symbols=a%2Bb%3Dc%26d&unicode=%E4%B8%AD%E6%96%87%E6%B5%8B%E8%AF%95'
```

### Custom Encoding

By default, characters requiring percent-encoding within the query string will
be encoded as UTF-8. If an alternative encoding is required, then an alternative
`encodeURIComponent` option will need to be specified:

```js
// Custom encoder with error handling
function safeEncoder(str) {
  try {
    return querystring.escape(str);
  } catch (err) {
    console.warn('Encoding failed for:', str);
    return '';
  }
}

const customEncoded = querystring.stringify(
  { message: 'Hello World', special: '特殊字符' },
  null,
  null,
  { encodeURIComponent: safeEncoder }
);
console.log(customEncoded);
```

## `querystring.unescape(str)`

<!-- YAML
added: v0.1.25
-->

* `str` {string}

The `querystring.unescape()` method performs decoding of URL percent-encoded
characters on the given `str`.

The `querystring.unescape()` method is used by `querystring.parse()` and is
generally not expected to be used directly. It is exported primarily to allow
application code to provide a replacement decoding implementation if
necessary by assigning `querystring.unescape` to an alternative function.

By default, the `querystring.unescape()` method will attempt to use the
JavaScript built-in `decodeURIComponent()` method to decode. If that fails,
a safer equivalent that does not throw on malformed URLs will be used.

### Examples

```js
const querystring = require('node:querystring');

// Basic URL decoding
console.log(querystring.unescape('Hello%20World')); // 'Hello World'
console.log(querystring.unescape('user%40example.com')); // 'user@example.com'
console.log(querystring.unescape('price%3D%24100')); // 'price=$100'

// Unicode characters
console.log(querystring.unescape('%E4%B8%AD%E6%96%87')); // '中文'

// Malformed sequences are handled safely (no exceptions thrown)
console.log(querystring.unescape('invalid%ZZ')); // 'invalid%ZZ' (unchanged)
console.log(querystring.unescape('partial%2')); // 'partial%2' (unchanged)

// Mixed valid and invalid sequences
console.log(querystring.unescape('Hello%20%ZZWorld')); // 'Hello %ZZWorld'
```

### Error Handling

```js
// Unlike decodeURIComponent(), querystring.unescape() never throws
try {
  const result1 = decodeURIComponent('invalid%ZZ'); // Throws URIError
} catch (err) {
  console.log('decodeURIComponent threw:', err.message);
}

// querystring.unescape() handles malformed input gracefully
const result2 = querystring.unescape('invalid%ZZ'); // Returns 'invalid%ZZ'
console.log('querystring.unescape result:', result2);
```

## Related APIs and Best Practices

### When to Use `querystring` vs `URLSearchParams`

```js
const querystring = require('node:querystring');
const { URLSearchParams } = require('node:url');

// querystring - Better performance, Node.js specific
const qsParsed = querystring.parse('name=John&age=30');
console.log(qsParsed); // { name: 'John', age: '30' }

// URLSearchParams - Web standard, better for browser compatibility
const urlParams = new URLSearchParams('name=John&age=30');
console.log(urlParams.get('name')); // 'John'
console.log([...urlParams]); // [['name', 'John'], ['age', '30']]
```

**Use `querystring` when:**
- Performance is critical
- Working in a Node.js-only environment
- Need simple parsing/stringifying
- Processing form data from HTTP requests

**Use `URLSearchParams` when:**
- Browser compatibility is important
- Need advanced query manipulation methods
- Working with modern web APIs
- Code needs to run in both Node.js and browsers

### Integration with Other Node.js APIs

```js
const querystring = require('node:querystring');
const url = require('node:url');
const http = require('node:http');

// Parsing URL query strings
const fullUrl = 'https://example.com/search?q=nodejs&category=docs&page=1';
const parsedUrl = url.parse(fullUrl, true); // true enables query parsing
console.log(parsedUrl.query); // { q: 'nodejs', category: 'docs', page: '1' }

// Manual parsing with querystring
const manualParsed = querystring.parse(parsedUrl.search.slice(1));
console.log(manualParsed); // Same result

// Building URLs with query parameters
const baseUrl = 'https://api.example.com/search';
const params = { q: 'nodejs', limit: 10, offset: 0 };
const queryStr = querystring.stringify(params);
const fullApiUrl = `${baseUrl}?${queryStr}`;
console.log(fullApiUrl); // 'https://api.example.com/search?q=nodejs&limit=10&offset=0'
```

### Security Considerations

```js
const querystring = require('node:querystring');

// Always validate and sanitize parsed data
function safeParseQuery(queryStr, maxKeys = 100) {
  const parsed = querystring.parse(queryStr, null, null, { maxKeys });
  
  // Validate each key-value pair
  const sanitized = {};
  for (const [key, value] of Object.entries(parsed)) {
    // Limit key length to prevent memory attacks
    if (key.length > 100) continue;
    
    // Handle arrays safely
    if (Array.isArray(value)) {
      sanitized[key] = value.slice(0, 10).map(v => String(v).slice(0, 1000));
    } else {
      sanitized[key] = String(value).slice(0, 1000);
    }
  }
  
  return sanitized;
}

// Example usage
const userInput = 'name=John&comments=This%20is%20a%20comment&tags=js&tags=node';
const safeData = safeParseQuery(userInput);
console.log(safeData);
```

### Common Patterns

```js
const querystring = require('node:querystring');

// Pattern 1: Form data processing
function processFormData(formBody) {
  const data = querystring.parse(formBody);
  
  // Convert single-item arrays to strings for convenience
  for (const [key, value] of Object.entries(data)) {
    if (Array.isArray(value) && value.length === 1) {
      data[key] = value[0];
    }
  }
  
  return data;
}

// Pattern 2: API query builder
function buildApiQuery(filters) {
  // Remove null/undefined values
  const cleanFilters = Object.fromEntries(
    Object.entries(filters).filter(([_, value]) => value != null)
  );
  
  return querystring.stringify(cleanFilters);
}

// Pattern 3: URL parameter extraction
function extractUrlParams(url) {
  const [, queryStr] = url.split('?');
  return queryStr ? querystring.parse(queryStr) : {};
}

// Usage examples
console.log(processFormData('name=John&tags=js&tags=node'));
console.log(buildApiQuery({ search: 'nodejs', page: 1, limit: null }));
console.log(extractUrlParams('https://example.com/search?q=test&page=2'));
```

For more information on URL handling, see:
- [URL API documentation](url.md)
- [HTTP module documentation](http.md) for handling form data
- [URLSearchParams documentation](url.md#class-urlsearchparams) for web-standard query handling
