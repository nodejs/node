# Docdown <sup>v1.0.0</sup>

<!-- div -->


<!-- div -->

## <a id="docdown"></a>`docdown`
* [`docdown`](#docdown$optionsarray)

<!-- /div -->


<!-- /div -->


<!-- div -->


<!-- div -->

## `docdown`

<!-- div -->

### <a id="docdown$optionsarray"></a>`docdown([$options=array()])`
<a href="#docdown$optionsarray">#</a> [&#x24C8;](https://github.com/jdalton/docdown/blob/master/docdown.php#L34 "View in source") [&#x24C9;][1]

Generates Markdown from JSDoc entries in a given file.

#### Arguments
1. `[$options=array()]` *(Array)*: The options array.

#### Returns
*(String)*: The generated Markdown.

#### Example
```php
// specify a file path
$markdown = docdown(array(
  // path to js file
  'path' => $filepath,
  // url used to reference line numbers in code
  'url' => 'https://github.com/username/project/blob/master/my.js'
));

// or pass raw js
$markdown = docdown(array(
  // raw JavaScript source
  'source' => $rawJS,
  // documentation title
  'title' => 'My API Documentation',
  // url used to reference line numbers in code
  'url' => 'https://github.com/username/project/blob/master/my.js'
));
```

* * *

<!-- /div -->


<!-- /div -->


<!-- /div -->


  [1]: #docdown "Jump back to the TOC."