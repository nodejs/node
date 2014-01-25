# columnify

[![Build Status](https://travis-ci.org/timoxley/columnify.png?branch=master)](https://travis-ci.org/timoxley/columnify)

Create text-based columns suitable for console output. 
Supports minimum and maximum column widths via truncation and text wrapping.

Designed to [handle sensible wrapping in npm search results](https://github.com/isaacs/npm/pull/2328).

`npm search` before & after integrating columnify:

![npm-tidy-search](https://f.cloud.github.com/assets/43438/1848959/ae02ad04-76a1-11e3-8255-4781debffc26.gif)

## Installation & Update

```
$ npm install --save columnify@latest
```

## Usage

```js
var columnify = require('columnify')
var columns = columnify(data, options)
console.log(columns)
```

## Examples

### Simple Columns

Text is aligned under column headings. Columns are automatically resized
to fit the content of the largest cell.  Each cell will be padded with
spaces to fill the available space and ensure column contents are
left-aligned.

```js
var columnify = require('columnify')

var columns = columnify([{
  name: 'mod1',
  version: '0.0.1'
}, {
  name: 'module2',
  version: '0.2.0'
}])

console.log(columns)
```
```
NAME    VERSION
mod1    0.0.1  
module2 0.2.0  
```

### Wrapping Column Cells

You can define the maximum width before wrapping for individual cells in
columns. Minimum width is also supported. Wrapping will happen at word
boundaries. Empty cells or those which do not fill the max/min width
will be padded with spaces.

```js
var columnify = require('columnify')

var columns = columnify([{
  name: 'mod1',
  description: 'some description which happens to be far larger than the max',
  version: '0.0.1',
}, {
  name: 'module-two',
  description: 'another description larger than the max',
  version: '0.2.0',
})

console.log(columns)
```
```
NAME       DESCRIPTION                    VERSION
mod1       some description which happens 0.0.1
           to be far larger than the max
module-two another description larger     0.2.0
           than the max
```

### Truncated Columns

You can disable wrapping and instead truncate content at the maximum
column width. Truncation respects word boundaries.  A truncation marker,
`…` will appear next to the last word in any truncated line.

```js
var columns = columnify(data, {
  truncate: true,
  config: {
    description: {
      maxWidth: 20
    }
  }
})

console.log(columns)
```

```
NAME       DESCRIPTION          VERSION
mod1       some description…    0.0.1  
module-two another description… 0.2.0  
```


### Custom Truncation Marker

You can change the truncation marker to something other than the default
`…`.

```js
var columns = columnify(data, {
  truncate: true,
  truncateMarker: '>',
  widths: {
    description: {
      maxWidth: 20
    }
  }
})

console.log(columns)
```

```
NAME       DESCRIPTION          VERSION
mod1       some description>    0.0.1  
module-two another description> 0.2.0  
```

### Custom Column Splitter

If your columns need some bling, you can split columns with custom
characters.

```js

var columns = columnify(data, {
  columnSplitter: ' | '
})

console.log(columns)
```
```
NAME       | DESCRIPTION                                                  | VERSION
mod1       | some description which happens to be far larger than the max | 0.0.1
module-two | another description larger than the max                      | 0.2.0
```

### Filtering & Ordering Columns

By default, all properties are converted into columns, whether or not
they exist on every object or not.

To explicitly specify which columns to include, and in which order,
supply an "include" array:

```js
var data = [{
  name: 'module1',
  description: 'some description',
  version: '0.0.1',
}, {
  name: 'module2',
  description: 'another description',
  version: '0.2.0',
}]

var columns = columnify(data, {
  include: ['name', 'version'] // note description not included
})

console.log(columns)
```

```
NAME    VERSION
module1 0.0.1
module2 0.2.0
```
## License

MIT
