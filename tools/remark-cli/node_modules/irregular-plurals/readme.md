# irregular-plurals [![Build Status](https://travis-ci.org/sindresorhus/irregular-plurals.svg?branch=master)](https://travis-ci.org/sindresorhus/irregular-plurals)

> Map of nouns to their irregular plural form  
>
> An irregular plural in this library is defined as a noun that cannot be made plural by applying these rules:
> - If the noun ends in an "s", "x", "z", "ch" or "sh", add "es"
> - If the noun ends in a "y" and is preceded by a consonent, drop the "y" and add "ies"
> - If the noun ends in a "y" and is preceded by a vowel, add "s"

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
