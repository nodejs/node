# columnify

[![Build Status](https://travis-ci.org/timoxley/columnify.png?branch=master)](https://travis-ci.org/timoxley/columnify)

Create text-based columns suitable for console output from objects or
arrays of objects.

Columns are automatically resized to fit the content of the largest
cell. Each cell will be padded with spaces to fill the available space
and ensure column contents are left-aligned.

Designed to [handle sensible wrapping in npm search results](https://github.com/isaacs/npm/pull/2328).

`npm search` before & after integrating columnify:

![npm-tidy-search](https://f.cloud.github.com/assets/43438/1848959/ae02ad04-76a1-11e3-8255-4781debffc26.gif)

## Installation & Update

```
$ npm install --save columnify@latest
```

## Usage

```javascript
var columnify = require('columnify')
var columns = columnify(data, options)
console.log(columns)
```

## Examples

### Columnify Objects

Objects are converted to a list of key/value pairs:

```javascript

var data = {
  "commander@0.6.1": 1,
  "minimatch@0.2.14": 3,
  "mkdirp@0.3.5": 2,
  "sigmund@1.0.0": 3
}

console.log(columnify(data))
```
#### Output:
```
KEY               VALUE
commander@0.6.1   1
minimatch@0.2.14  3
mkdirp@0.3.5      2
sigmund@1.0.0     3
```

### Custom Column Names

```javascript
var data = {
  "commander@0.6.1": 1,
  "minimatch@0.2.14": 3,
  "mkdirp@0.3.5": 2,
  "sigmund@1.0.0": 3
}

console.log(columnify(data, {columns: ['MODULE', 'COUNT']}))
```
#### Output:
```
MODULE            COUNT
commander@0.6.1   1
minimatch@0.2.14  3
mkdirp@0.3.5      2
sigmund@1.0.0     3
```

### Columnify Arrays of Objects

Column headings are extracted from the keys in supplied objects.

```javascript
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
#### Output:
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

```javascript

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
#### Output:
```
NAME       DESCRIPTION                    VERSION
mod1       some description which happens 0.0.1
           to be far larger than the max
module-two another description larger     0.2.0
           than the max
```

### Truncating Column Cells

You can disable wrapping and instead truncate content at the maximum
column width. Truncation respects word boundaries.  A truncation marker,
`…` will appear next to the last word in any truncated line.

```javascript
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
#### Output:
```
NAME       DESCRIPTION          VERSION
mod1       some description…    0.0.1  
module-two another description… 0.2.0  
```

### Filtering & Ordering Columns

By default, all properties are converted into columns, whether or not
they exist on every object or not.

To explicitly specify which columns to include, and in which order,
supply a "columns" or "include" array ("include" is just an alias).

```javascript
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
  columns: ['name', 'version'] // note description not included
})

console.log(columns)
```

#### Output:
```
NAME    VERSION
module1 0.0.1
module2 0.2.0
```


## Other Configuration Options

### Align Right

```js
var data = {
  "mocha@1.18.2": 1,
  "commander@2.0.0": 1,
  "debug@0.8.1": 1
}

columnify(data, {config: {value: {align: 'right'}}})
```

####  Output:
```
KEY                  VALUE
mocha@1.18.2             1
commander@2.0.0          1
debug@0.8.1              1
```

### Preserve existing newlines

By default, `columnify` sanitises text by replacing any occurance of 1 or more whitespace characters with a single space.

`columnify` can be configured to respect existing new line characters using the `preserveNewLines` option. Note this will still collapse all other whitespace.

```javascript
var data = [{
  name: "glob@3.2.9",
  paths: [
    "node_modules/tap/node_modules/glob",
    "node_modules/tape/node_modules/glob"
  ].join('\n')
}, {
  name: "nopt@2.2.1",
  paths: [
    "node_modules/tap/node_modules/nopt"
  ]
}, {
  name: "runforcover@0.0.2",
  paths: "node_modules/tap/node_modules/runforcover"
}]

console.log(columnify(data, {preserveNewLines: true}))
```
#### Output:
```
NAME              PATHS
glob@3.2.9        node_modules/tap/node_modules/glob
                  node_modules/tape/node_modules/glob
nopt@2.2.1        node_modules/tap/node_modules/nopt
runforcover@0.0.2 node_modules/tap/node_modules/runforcover
```

Compare this with output without `preserveNewLines`:

```javascript
console.log(columnify(data, {preserveNewLines: false}))
// or just
console.log(columnify(data))
```

```
NAME              PATHS
glob@3.2.9        node_modules/tap/node_modules/glob node_modules/tape/node_modules/glob
nopt@2.2.1        node_modules/tap/node_modules/nopt
runforcover@0.0.2 node_modules/tap/node_modules/runforcover
```

### Custom Truncation Marker

You can change the truncation marker to something other than the default
`…`.

```javascript
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
#### Output:
```
NAME       DESCRIPTION          VERSION
mod1       some description>    0.0.1  
module-two another description> 0.2.0  
```

### Custom Column Splitter

If your columns need some bling, you can split columns with custom
characters.

```javascript

var columns = columnify(data, {
  columnSplitter: ' | '
})

console.log(columns)
```
#### Output:
```
NAME       | DESCRIPTION                                                  | VERSION
mod1       | some description which happens to be far larger than the max | 0.0.1
module-two | another description larger than the max                      | 0.2.0
```

## Multibyte Character Support

`columnify` uses [mycoboco/wcwidth.js](https://github.com/mycoboco/wcwidth.js) to calculate length of multibyte characters:

```javascript
var data = [{
  name: 'module-one',
  description: 'some description',
  version: '0.0.1',
}, {
  name: '这是一个很长的名字的模块',
  description: '这真的是一个描述的内容这个描述很长',
  version: "0.3.3"
}]

console.log(columnify(data))
```

#### Without multibyte handling:

i.e. before columnify added this feature

```
NAME         DESCRIPTION       VERSION
module-one   some description  0.0.1
这是一个很长的名字的模块 这真的是一个描述的内容这个描述很长 0.3.3
```

#### With multibyte handling:

```
NAME                     DESCRIPTION                        VERSION
module-one               some description                   0.0.1
这是一个很长的名字的模块 这真的是一个描述的内容这个描述很长 0.3.3
```

## License

MIT


