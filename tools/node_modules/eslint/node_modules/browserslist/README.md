# Browserslist [![Cult Of Martians][cult-img]][cult]

<img width="120" height="120" alt="Browserslist logo by Anton Lovchikov"
     src="https://browserslist.github.io/browserslist/logo.svg" align="right">

The config to share target browsers and Node.js versions between different
front-end tools. It is used in:

* [Autoprefixer]
* [Babel]
* [postcss-preset-env]
* [eslint-plugin-compat]
* [stylelint-no-unsupported-browser-features]
* [postcss-normalize]
* [obsolete-webpack-plugin]

All tools will find target browsers automatically,
when you add the following to `package.json`:

```json
  "browserslist": [
    "defaults",
    "not IE 11",
    "maintained node versions"
  ]
```

Or in `.browserslistrc` config:

```yaml
# Browsers that we support

defaults
not IE 11
maintained node versions
```

Developers set their version lists using queries like `last 2 versions`
to be free from updating versions manually.
Browserslist will use [`caniuse-lite`] with [Can I Use] data for this queries.

Browserslist will take queries from tool option,
`browserslist` config, `.browserslistrc` config,
`browserslist` section in `package.json` or environment variables.

[cult-img]: https://cultofmartians.com/assets/badges/badge.svg
[cult]: https://cultofmartians.com/done.html

<a href="https://evilmartians.com/?utm_source=browserslist">
  <img src="https://evilmartians.com/badges/sponsored-by-evil-martians.svg"
       alt="Sponsored by Evil Martians" width="236" height="54">
</a>

[stylelint-no-unsupported-browser-features]: https://github.com/ismay/stylelint-no-unsupported-browser-features
[eslint-plugin-compat]:                      https://github.com/amilajack/eslint-plugin-compat
[Browserslist Example]:                      https://github.com/browserslist/browserslist-example
[postcss-preset-env]:                        https://github.com/jonathantneal/postcss-preset-env
[postcss-normalize]:                         https://github.com/jonathantneal/postcss-normalize
[`caniuse-lite`]:                            https://github.com/ben-eb/caniuse-lite
[Autoprefixer]:                              https://github.com/postcss/autoprefixer
[Can I Use]:                                 https://caniuse.com/
[Babel]:                                     https://github.com/babel/babel/tree/master/packages/babel-preset-env
[obsolete-webpack-plugin]:                   https://github.com/ElemeFE/obsolete-webpack-plugin

## Docs
Read **[full docs](https://github.com/browserslist/browserslist#readme)** on GitHub.
