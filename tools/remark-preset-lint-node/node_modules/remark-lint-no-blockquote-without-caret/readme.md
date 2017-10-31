<!--This file is generated-->

# remark-lint-no-blockquote-without-caret

Warn when blank lines without carets are found in a blockquote.

## Install

```sh
npm install --save remark-lint-no-blockquote-without-caret
```

## Example

When this rule is turned on, the following file
`valid.md` is ok:

```markdown
> Foo...
>
> ...Bar.
```

When this rule is turned on, the following file
`invalid.md` is **not** ok:

```markdown
> Foo...

> ...Bar.
```

```text
2:1: Missing caret in blockquote
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
