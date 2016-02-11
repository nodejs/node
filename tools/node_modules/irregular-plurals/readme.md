# irregular-plurals [![Build Status](https://travis-ci.org/sindresorhus/irregular-plurals.svg?branch=master)](https://travis-ci.org/sindresorhus/irregular-plurals)

> Map of nouns to their irregular plural form  
> Irregular plurals are nouns that cannot be made plural by adding an "s" or "es" to the end

*The list is just a [JSON file](irregular-plurals.json) and can be used wherever.*


## Install

```
$ npm install --save irregular-plurals
```


## Usage

```js
const irregularPlurals = require('irregular-plurals');

console.log(irregularPlurals['cactus']);
//=> 'cacti'

console.log(irregularPlurals);
/*
    {
        addendum: 'addenda',
        alga: 'algae',
        ...
    }
*/
```


## Related

- [plur](https://github.com/sindresorhus/plur) - Pluralize a word


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
