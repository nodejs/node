# remark-lint-prohibited-strings
remark-lint plugin to prohibit specified strings in markdown files

Example configuration:

```javascript
{
  "plugins": [
    "remark-lint-prohibited-strings",
    [
      { no: "End-Of-Life", yes: "End-of-Life" },
      { no: "End-of-life", yes: "End-of-Life" },
      { no: 'gatsby', yes: "Gatsby", ignoreNextTo: "-" },
      { no: "Github", yes: "GitHub" },
      { no: "Javascript", yes: "JavaScript" },
      { no: "Node.JS", yes: "Node.js" },
      { no: "Rfc", yes: "RFC" },
      { no: "[Rr][Ff][Cc]\\d+", yes: "RFC <number>" },
      { no: "rfc", yes: "RFC" },
      { no: "UNIX", yes: "Unix" },
      { no: "unix", yes: "Unix" },
      { no: "v8", yes: "V8" }
    ]
  ]
}
  ```

`no` is a string specifying the string you wish to prohibit. Regular expression
characters are respected. If `no` is omitted but `yes` is supplied, then the
`no` string will be inferred to be any case-insensitive match of the `yes`
string that is not a case-sensitive match of the `yes` string. In other words,
`{ yes: 'foo' }` means that _foo_ is permitted, but _Foo_ and _FOO_ are
prohibited.

`yes` is an optional string specifying what someone will be told to use instead
of the matched `no` value.

`ignoreNextTo` is a string that will make a prohibited string allowable if it
appears next to that string. For example, in the configuration above, _gatsby_
will be flagged as a problem and the user will be told to use _Gatsby_ instead.
However, _gatsby-plugin_ will not be flagged because `'-'` is included in
`ignoreNextTo` for that rule.

If `replaceCaptureGroups` is set to a truthy value, the message reported to the
user will have capture groups and other replacements supplied from the regular
expression match using the same process as the replacement string in
`String.prototype.replace()`. It defaults to `false`. For example,
`{ no: "[Rr][Ff][Cc](\\d+)", yes: "RFC $1" }` will tell the user to use
`"RFC $1"` by default. With `replaceCapture` set to `true`, it would instead
tell the user to (for example) use `"RFC 123"` instead of `"rfc123"`.

