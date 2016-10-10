# eslint-plugin-standard
ESlint Rules for the Standard Linter

### Usage

`npm install --save-dev eslint-plugin-standard`

### Configuration

```js
{
  rules: {
    'standard/object-curly-even-spacing': [2, "either"]
    'standard/array-bracket-even-spacing': [2, "either"],
    'standard/computed-property-even-spacing': [2, "even"]
  }
}
```

### Rules Explanations

There are several rules that were created specifically for the `standard` linter.

- `object-curly-even-spacing` - Like `object-curly-spacing` from ESLint except it has an `either` option which lets you have 1 or 0 spaces padding.
- `array-bracket-even-spacing` - Like `array-bracket-even-spacing` from ESLint except it has an `either` option which lets you have 1 or 0 spacing padding.
- `computed-property-even-spacing` - Like `computed-property-spacing` around ESLint except is has an `even` option which lets you have 1 or 0 spacing padding.

