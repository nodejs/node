<!--remark setext-->

<!--lint disable no-multiple-toplevel-headings maximum-line-length-->

3.2.2 / 2016-01-21
==================

*   Refactor internal ignore API in remark(1) ([`c94c7df`](https://github.com/wooorm/remark/commit/c94c7df))

3.2.1 / 2016-01-19
==================

*   Fix empty list-item bug in remark(3) ([`8a62652`](https://github.com/wooorm/remark/commit/8a62652))
*   Add support for default arguments to remark(1) ([`9ab6dc8`](https://github.com/wooorm/remark/commit/9ab6dc8))
*   Fix injecting virtual files on remark(1) ([`397028a`](https://github.com/wooorm/remark/commit/397028a))
*   Add natural-language validation ([`d1c52a3`](https://github.com/wooorm/remark/commit/d1c52a3))

3.2.0 / 2016-01-10
==================

*   Fix void list-items in remark(3) ([`6a42de4`](https://github.com/wooorm/remark/commit/6a42de4))
*   Add Gitter badge to `readme.md` ([`4cc1109`](https://github.com/wooorm/remark/commit/4cc1109))
*   Fix tables without final newline in remark(3) ([`fd2f281`](https://github.com/wooorm/remark/commit/fd2f281))

3.1.3 / 2016-01-04
==================

*   Fix table without body support to remark(3) ([`9a3bc47`](https://github.com/wooorm/remark/commit/9a3bc47))
*   Refactor output of `-h, --help` in remark(1) ([`1d36801`](https://github.com/wooorm/remark/commit/1d36801))
*   Add docs for fileglob support in remark(1) ([`f9f9a36`](https://github.com/wooorm/remark/commit/f9f9a36))

3.1.2 / 2016-01-03
==================

*   Fix globs to files without extension in remark(1) ([`20e253f`](https://github.com/wooorm/remark/commit/20e253f))

3.1.1 / 2016-01-02
==================

*   Update copyright date references to 2016 ([`48151aa`](https://github.com/wooorm/remark/commit/48151aa))
*   Fix stdin(4) detection on Windows ([`c2562bc`](https://github.com/wooorm/remark/commit/c2562bc))
*   Add `remark-license`, `remark-vdom` to list of plugins ([`6ca52bb`](https://github.com/wooorm/remark/commit/6ca52bb))

3.1.0 / 2015-12-31
==================

*   Add support for injecting plugins into remark(1) ([`4e65d70`](https://github.com/wooorm/remark/commit/4e65d70))

3.0.1 / 2015-12-27
==================

*   Fix files without extension in remark(1) ([`6d8bcd5`](https://github.com/wooorm/remark/commit/6d8bcd5))
*   Refactor remark(1) examples in docs to be valid ([`fb0cbe8`](https://github.com/wooorm/remark/commit/fb0cbe8))
*   Add `remark-textr` to list of plugins ([`098052a`](https://github.com/wooorm/remark/commit/098052a))
*   Update list of plugins with renames ([`caf5232`](https://github.com/wooorm/remark/commit/caf5232))

3.0.0 / 2015-12-25
==================

*   Add migration guide and temporary warnings ([`269f521`](https://github.com/wooorm/remark/commit/269f521))
*   Update list of plugins with project renames ([`fb0fea9`](https://github.com/wooorm/remark/commit/fb0fea9))
*   Rename `mdast` to `remark` ([`38fe53d`](https://github.com/wooorm/remark/commit/38fe53d))
*   Fix bug where blockquotes had trailing spaces ([`a51f112`](https://github.com/wooorm/remark/commit/a51f112))
*   Refactor support for entities ([`0c7b649`](https://github.com/wooorm/remark/commit/0c7b649))
*   Refactor code-style ([`3dc2485`](https://github.com/wooorm/remark/commit/3dc2485))
*   Add documentation for interfacing with the parser ([`7a5d16d`](https://github.com/wooorm/remark/commit/7a5d16d))
*   Fix footnote definitions without spacing ([`46714b2`](https://github.com/wooorm/remark/commit/46714b2))
*   Fix empty `alt` value for `imageReference`, `image` ([`698d569`](https://github.com/wooorm/remark/commit/698d569))
*   Fix unclosed angle-bracketed definition ([`acebf81`](https://github.com/wooorm/remark/commit/acebf81))
*   Add escaping to compiler ([`d1fe019`](https://github.com/wooorm/remark/commit/d1fe019))
*   Fix handling of definitions in commonmark-mode ([`b7d6e53`](https://github.com/wooorm/remark/commit/b7d6e53))
*   Fix handling of list-item bullets in gfm-mode ([`6e74759`](https://github.com/wooorm/remark/commit/6e74759))
*   Refactor to remove regexes ([`25a26f2`](https://github.com/wooorm/remark/commit/25a26f2))

2.3.2 / 2015-12-21
==================

*   Fix missing link in `history.md` ([`49453f7`](https://github.com/wooorm/remark/commit/49453f7))

2.3.1 / 2015-12-21
==================

*   Fix `package.json` files not loading on mdast(1) ([`1ef6e05`](https://github.com/wooorm/remark/commit/1ef6e05))

2.3.0 / 2015-12-01
==================

*   Refactor to prefer prefixed plug-ins in mdast(1) ([`44d4daa`](https://github.com/wooorm/remark/commit/44d4daa))
*   Fix recursive `plugins` key handling in mdast(1) ([`4bb02b2`](https://github.com/wooorm/remark/commit/4bb02b2))
*   Add getting-started documentation ([`a20d9d0`](https://github.com/wooorm/remark/commit/a20d9d0))
*   Fix stdout(4) and stderr(4) usage ([`501a377`](https://github.com/wooorm/remark/commit/501a377))

2.2.2 / 2015-11-21
==================

*   Fix premature reporting ([`a82d018`](https://github.com/wooorm/remark/commit/a82d018))
*   Remove distribution files from version control ([`f068edd`](https://github.com/wooorm/remark/commit/f068edd))
*   Remove support for bower ([`61162be`](https://github.com/wooorm/remark/commit/61162be))

2.2.1 / 2015-11-20
==================

*   Fix silent exit when without `.mdastignore` file ([`7233fb2`](https://github.com/wooorm/remark/commit/7233fb2))

2.2.0 / 2015-11-19
==================

*   Update dependencies ([`c2aca8f`](https://github.com/wooorm/remark/commit/c2aca8f))
*   Refactor to externalise file finding ([`517d483`](https://github.com/wooorm/remark/commit/517d483))
*   Refactor to make node to `encode` optional ([`03c3361`](https://github.com/wooorm/remark/commit/03c3361))
*   Add version to library, distribution files ([`e245a43`](https://github.com/wooorm/remark/commit/e245a43))
*   Remove output to stdout(4) when watching ([`594a45f`](https://github.com/wooorm/remark/commit/594a45f))
*   Remove variable capturing groups from regexes ([`c17228c`](https://github.com/wooorm/remark/commit/c17228c))

2.1.0 / 2015-10-05
==================

*   Fix pipes inside code in tables ([`baad536`](https://github.com/wooorm/remark/commit/baad536))
*   Add processing of just the changed watched file to mdast(1) ([`e3748d2`](https://github.com/wooorm/remark/commit/e3748d2))
*   Add error when pedantic is used with incompatible options ([`3326a89`](https://github.com/wooorm/remark/commit/3326a89))

2.0.0 / 2015-09-20
==================

*   Update unified ([`3dec23f`](https://github.com/wooorm/remark/commit/3dec23f))
*   Update AUTHORS with recent contributors ([`49ec46f`](https://github.com/wooorm/remark/commit/49ec46f))
*   Add support for watching files for changes to mdast(1) ([`b3078f9`](https://github.com/wooorm/remark/commit/b3078f9))
*   Fix stdin detection in child-processes ([`8f1d7f1`](https://github.com/wooorm/remark/commit/8f1d7f1))
*   Add list of tools built with mdast ([`d7ad2ac`](https://github.com/wooorm/remark/commit/d7ad2ac))

1.2.0 / 2015-09-07
==================

*   Fix bug where colour on mdast(1) persisted ([`ca2c53a`](https://github.com/wooorm/remark/commit/ca2c53a))
*   Add support for loading global plugins ([`5e2d32e`](https://github.com/wooorm/remark/commit/5e2d32e))
*   Add more mdastplugin(3) examples ([`8664a03`](https://github.com/wooorm/remark/commit/8664a03))
*   Move all info output of mdast(1) to stderr(4) ([`daed42f`](https://github.com/wooorm/remark/commit/daed42f))
*   Refactor miscellaneous docs ([`72412af`](https://github.com/wooorm/remark/commit/72412af))

1.1.0 / 2015-08-21
==================

*   Fix typo in man-page ([`d435999`](https://github.com/wooorm/remark/commit/d435999))
*   Update list of plug-ins ([`4add9e6`](https://github.com/wooorm/remark/commit/4add9e6))
*   Refactor to externalise reporter ([`8c80af9`](https://github.com/wooorm/remark/commit/8c80af9))
*   Refactor mdast(1) to externalise `to-vfile` ([`69629d5`](https://github.com/wooorm/remark/commit/69629d5))

1.0.0 / 2015-08-12
==================

Nothing much changed, just that **mdast** is now officially stable!
Thanks all for the 100 stars :wink:.

The pre-stable change-log was removed from the working-copy.
View the changelog at [`1.0.0`](https://github.com/wooorm/remark/blob/1.0.0/history.md)
