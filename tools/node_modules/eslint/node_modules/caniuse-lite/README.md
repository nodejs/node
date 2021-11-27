# caniuse-lite

A smaller version of caniuse-db, with only the essentials!

## Why?

The full data behind [Can I use][1] is incredibly useful for any front end
developer, and on the website all of the details from the database are displayed
to the user. However in automated tools, [many of these fields go unused][2];
it's not a problem for server side consumption but client side, the less
JavaScript that we send to the end user the better.

caniuse-lite then, is a smaller dataset that keeps essential parts of the data
in a compact format. It does this in multiple ways, such as converting `null`
array entries into empty strings, representing support data as an integer rather
than a string, and using base62 references instead of longer human-readable
keys.

This packed data is then reassembled (via functions exposed by this module) into
a larger format which is mostly compatible with caniuse-db, and so it can be
used as an almost drop-in replacement for caniuse-db for contexts where size on
disk is important; for example, usage in web browsers. The API differences are
very small and are detailed in the section below.


## API

```js
import * as lite from 'caniuse-lite';
```


### `lite.agents`

caniuse-db provides a full `data.json` file which contains all of the features
data. Instead of this large file, caniuse-lite provides this data subset
instead, which has the `browser`, `prefix`, `prefix_exceptions`, `usage_global`
and `versions` keys from the original.

In addition, the subset contains the `release_date` key with release dates (as timestamps) for each version:
```json
{
  "release_date": {
    "6": 998870400,
    "7": 1161129600,
    "8": 1237420800,
    "9": 1300060800,
    "10": 1346716800,
    "11": 1381968000,
    "5.5": 962323200
  }
}
```


### `lite.feature(js)`

The `feature` method takes a file from `data/features` and converts it into
something that more closely represents the `caniuse-db` format. Note that only
the `title`, `stats` and `status` keys are kept from the original data.


### `lite.features`

The `features` index is provided as a way to query all of the features that
are listed in the `caniuse-db` dataset. Note that you will need to use the
`feature` method on values from this index to get a human-readable format.


### `lite.region(js)`

The `region` method takes a file from `data/regions` and converts it into
something that more closely represents the `caniuse-db` format. Note that *only*
the usage data is exposed here (the `data` key in the original files).


## License

The data in this repo is available for use under a CC BY 4.0 license
(http://creativecommons.org/licenses/by/4.0/). For attribution just mention
somewhere that the source is caniuse.com. If you have any questions about using
the data for your project please contact me here: http://a.deveria.com/contact

[1]: http://caniuse.com/
[2]: https://github.com/Fyrd/caniuse/issues/1827


## Security contact information

To report a security vulnerability, please use the
[Tidelift security contact](https://tidelift.com/security).
Tidelift will coordinate the fix and disclosure.
