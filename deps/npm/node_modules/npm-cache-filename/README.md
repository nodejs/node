# npm-cache-filename

Given a cache folder and url, return the appropriate cache folder.

## USAGE

```javascript
var cf = require('npm-cache-filename');
console.log(cf('/tmp/cache', 'https://registry.npmjs.org:1234/foo/bar'));
// outputs: /tmp/cache/registry.npmjs.org_1234/foo/bar
```

As a bonus, you can also bind it to a specific root path:

```javascript
var cf = require('npm-cache-filename');
var getFile = cf('/tmp/cache');

console.log(getFile('https://registry.npmjs.org:1234/foo/bar'));
// outputs: /tmp/cache/registry.npmjs.org_1234/foo/bar
```
