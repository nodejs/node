# plur [![Build Status](https://travis-ci.org/sindresorhus/plur.svg?branch=master)](https://travis-ci.org/sindresorhus/plur)

> Pluralize a word


## Install

```
$ npm install --save plur
```


## Usage

```js
const plur = require('plur');

plur('unicorn', 4);
//=> 'unicorns'

plur('puppy', 2);
//=> 'puppies'

plur('box', 2);
//=> 'boxes'

plur('cactus', 2);
//=> 'cacti'
```


## API

### plur(word, [plural], count)

#### word

Type: `string`

Word to pluralize.

#### plural

Type: `string`  
Default:

- Irregular nouns will use this [list](https://github.com/sindresorhus/irregular-plurals/blob/master/irregular-plurals.json).
- Words ending in *s*, *x*, *z*, *ch*, *sh* will be pluralized with *-es* (eg. *foxes*).
- Words ending in *y* that are preceded by a consonant will be pluralized by replacing *y* with *-ies* (eg. *puppies*).
- All other words will have "s" added to the end (eg. *days*).

Pluralized word.

The plural suffix will match the case of the last letter in the word.

This option is only for extreme edge-cases. You probably won't need it.

#### count

Type: `number`

Count to determine whether to use singular or plural.


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
