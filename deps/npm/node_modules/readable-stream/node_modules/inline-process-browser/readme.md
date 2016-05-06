inline-process-browser
===

Browserify transform which turns any reference to `process.browser` into `true`.

Can remove non-browser code when combined with [unreachable-branch-transform](https://github.com/zertosh/unreachable-branch-transform)


turns

```js
if (process.browser) {
  //something
}
```

into

```js
if (true) {
  //something
}
```
