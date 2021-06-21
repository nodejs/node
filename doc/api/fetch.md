# Fetch

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental

## Class: `fetch.Headers`

Represents a WHATWG Fetch Spec
[Headers Class](https://fetch.spec.whatwg.org/#headers-class)

### `new Headers([init])`

* `init` {Headers | Iterable<[string, string]> | string[] | Record<string,
  string>} Initial header list to be cloned into the new instance

```js
new Headers();

new Headers([['name', 'value']]);

new Headers(['name', 'value']);

const headers = new Headers({
  name: 'value',
});

new Headers(headers);
```

### `headers.append(name, value)`

* `name` {string}
* `value` {string}
* Returns: {void}

Non-destructive operation for adding header entries. When called multiple times
with the same _name_, the values will be collected in a list and returned
together when retrieved using [Headers.get](#headersgetname).

```js
const headers = new Headers();

headers.append('undici', 'fetch');
headers.get('undici'); // -> 'fetch'

headers.append('foobar', 'fuzz');
headers.append('foobar', 'buzz');
headers.get('foobar'); // -> 'fuzz, buzz'
```

### `headers.delete(name)`

* `name` {string}

Removes a header entry. This operation is destructive and cannot be restored.
Does **not** throw an error if the given _name_ does not exist. Reminder that
[Headers.get](#headersgetname) will return `null` if the _name_ does not exist.

```js
const headers = new Headers();

headers.append('undici', 'fetch');

headers.get('undici'); // -> 'fetch'

headers.delete('undici');

headers.get('undici'); // -> null
```

### `headers.get(name)`

* `name` {string}
* Returns: {string | null}

Retrieves a header entry. If the entry _name_ has multiple values, they are
returned as a string joined by `','` characters. If the _name_ does not exist,
this method returns null.

```js
const headers = new Headers();

headers.append('undici', 'fetch');
headers.get('undici'); // -> 'fetch'

headers.append('foobar', 'fuzz');
headers.append('foobar', 'buzz');
headers.get('foobar'); // -> 'fuzz, buzz'

headers.get('nodejs'); // -> null
```

### `headers.has(name)`

* `name` {string}
* Returns: {boolean}

Checks for the existence of a given entry _name_.

```js
const headers = new Headers();

headers.append('undici', 'fetch');
headers.has('undici'); // -> true
```

### `headers.set(name, value)`

* `name` {string}
* `value` {string}

Destructive operation that will override any existing values for the given entry
_name_. For a non-destructive alternative see
[Headers.append](#headersappendname-value).

```js
const headers = new Headers();

headers.set('foobar', 'fuzz');
headers.get('foobar'); // -> 'fuzz'

headers.set('foobar', 'buzz');
headers.get('foobar'); // -> 'buzz'
```

### `headers.values()`

* Returns: {IteratableIterator<string>}

Yields a list of header values combined and sorted by their respective keys.

```js
const headers = new Headers();

headers.set('abc', '123');
headers.set('def', '456');
headers.set('ghi', '789');
headers.append('ghi', '012');

for (const value of headers.values()) {
  console.log(value);
}

// -> '123'
// -> '456'
// -> '789, 012'
```

### `headers.keys()`

Returns: {IteratableIterator<string>}

Yields a sorted list of header keys.

```js
const headers = new Headers();

headers.set('abc', '123');
headers.set('def', '456');
headers.set('ghi', '789');
headers.append('ghi', '012');

for (const name of headers.keys()) {
  console.log(name);
}

// -> 'abc'
// -> 'def'
// -> 'ghi'
```

### `headers.forEach(callback, [thisArg])`

* `callback` {(value: string, key: string, iterable: Headers) => void}
* `thisArg` {any} (optional)

A Headers class can be iterated using `.forEach(callback, [thisArg])`.

Optionally a `thisArg` can be passed which will be assigned to the `this`
context of callback.

The headers are returned in a sorted order, and values are combined on similar
keys.

```js
const headers = new Headers([['abc', '123']]);

headers.forEach(function(value, key, headers) {
  console.log(key, value);
});
// -> 'abc', '123'
```

### `headers[Symbol.iterator]`

* Returns: {Iterator<[string, string]>}

A Headers class instance is iterable. It yields each of its entries as a pair
where the first value is the entry _name_ and the second value is the header
_value_. They are sorted by _name_ or otherwise referred to as the header key.

```js
const headers = new Headers();

headers.set('abc', '123');
headers.set('def', '456');
headers.set('ghi', '789');
headers.append('ghi', '012');

for (const [name, value] of headers) {
  console.log(name, value);
}

// -> 'abc', '123'
// -> 'def', '456'
// -> 'ghi', '789, 012'
```

### `headers.entries()`

* Returns: {IteratableIterator<[string, string]>}

Yields a list of headers sorted and combined by key.

```js
const headers = new Headers();

headers.set('abc', '123');
headers.set('def', '456');
headers.set('ghi', '789');
headers.append('ghi', '012');

for (const entry of headers.entries()) {
  console.log(entry);
}

// -> 'abc', '123'
// -> 'def', '456'
// -> 'ghi', '789, 012'
```
