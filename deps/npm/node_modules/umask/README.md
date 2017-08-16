# umask

Convert umask from string &lt;-> number.

## Installation & Use

```
$ npm install -S umask

var umask = require('umask');

console.log(umask.toString(18));        // 0022

console.log(umask.fromString('0777'))   // 511
```

## API

### `toString( val )`

Converts `val` to a 0-padded octal string.  `val` is assumed to be a
Number in the correct range (0..511)

### `fromString( val, [cb] )`

Converts `val` to a Number that can be used as a umask.  `val` can
be of the following forms:

  * String containing octal number (leading 0)
  * String containing decimal number
  * Number

In all cases above, the value obtained is then converted to an integer and
checked against the legal `umask` range 0..511

`fromString` can be used as a simple converter, with no error feedback, by
omitting the optional callback argument `cb`:

```
   var mask = umask.fromString(val);

   // mask is now the umask descibed by val or
   // the default, 0022 (18 dec)
```

The callback arguments are `(err, val)` where `err` is either `null` or an
Error object and `val` is either the converted umask or the default umask, `0022`.

```
   umask.fromString(val, function (err, val) {
       if (err) {
          console.error("invalid umask: " + err.message)
       }

       /* do something with val */
   });
```

The callback, if provided, is always called **synchronously**.

### `validate( data, k, val )`

This is a validation function of the form expected by `nopt`.  If
`val` is a valid umask, the function returns true and sets `data[k]`.
If `val` is not a valid umask, the function returns false.

The `validate` function is stricter than `fromString`: it only accepts
Number or octal String values, and the String value must begin with `0`.
The `validate` function does **not** accept Strings containing decimal
numbers.

# Maintainer

Sam Mikes <smikes@cubane.com>

# License

MIT