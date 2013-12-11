# Docdown <sup>v1.0.0</sup>

A simple JSDoc to Markdown documentation generator.

## Documentation

The documentation for Docdown can be viewed here: [/doc/README.md](https://github.com/jdalton/docdown/blob/master/doc/README.md#readme)

For a list of upcoming features, check out our [roadmap](https://github.com/jdalton/docdown/wiki/Roadmap).

## Installation and usage

Usage example:

```php
require("docdown.php");

// generate Markdown
$markdown = docdown(array(
  "path" => $filepath,
  "url"  => "https://github.com/username/project/blob/master/my.js"
));
```

## Author

| [![twitter/jdalton](http://gravatar.com/avatar/299a3d891ff1920b69c364d061007043?s=70)](http://twitter.com/jdalton "Follow @jdalton on Twitter") |
|---|
| [John-David Dalton](http://allyoucanleet.com/) |

## Contributors

| [![twitter/mathias](http://gravatar.com/avatar/24e08a9ea84deb17ae121074d0f17125?s=70)](http://twitter.com/mathias "Follow @mathias on Twitter") |
|---|
| [Mathias Bynens](http://mathiasbynens.be/) |
