# parse-conflict-json

Parse a JSON string that has git merge conflicts, resolving if possible.

If the JSON is valid, it just does `JSON.parse` as normal.

If either side of the conflict is invalid JSON, then an error is thrown for
that.

## USAGE

```js
// after a git merge that left some conflicts there
const data = fs.readFileSync('package-lock.json', 'utf8')

// reviverFunction is passed to JSON.parse as the reviver function
// preference defaults to 'ours', set to 'theirs' to prefer the other
// side's changes.
const parsed = parseConflictJson(data, reviverFunction, preference)

// returns true if the data looks like a conflicted diff file
parsed.isDiff(data)
```

## Algorithm

If `prefer` is set to `theirs`, then the vaules of `theirs` and `ours` are
switched in the resolver function.  (Ie, we'll apply their changes on top
of our object, rather than the other way around.)

- Parse the conflicted file into 3 pieces: `ours`, `theirs`, and `parent`

- Get the [diff](https://github.com/angus-c/just#just-diff) from `parent`
  to `ours`.

- [Apply](https://github.com/angus-c/just#just-diff-apply) each change of
  that diff to `theirs`.

    If any change in the diff set cannot be applied (ie, because they
    changed an object into a non-object and we changed a field on that
    object), then replace the object at the specified path with the object
    at the path in `ours`.
