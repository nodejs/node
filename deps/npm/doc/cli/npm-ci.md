npm-ci(1) -- Install a project with a clean slate
===================================

## SYNOPSIS

    npm ci

## EXAMPLE

Make sure you have a package-lock and an up-to-date install:

```
$ cd ./my/npm/project
$ npm install
added 154 packages in 10s
$ ls | grep package-lock
```

Run `npm ci` in that project

```
$ npm ci
added 154 packages in 5s
```

Configure Travis to build using `npm ci` instead of `npm install`:

```
# .travis.yml
install:
- npm ci
# keep the npm cache around to speed up installs
cache:
  directories:
  - "$HOME/.npm"
```

## DESCRIPTION

This command is similar to `npm-install(1)`, except it's meant to be used in
automated environments such as test platforms, continuous integration, and
deployment. It can be significantly faster than a regular npm install by
skipping certain user-oriented features. It is also more strict than a regular
install, which can help catch errors or inconsistencies caused by the
incrementally-installed local environments of most npm users.

In short, the main differences between using `npm install` and `npm ci` are:

* The project **must** have an existing `package-lock.json` or `npm-shrinkwrap.json`.
* If dependencies in the package lock do not match those in `package.json`, `npm ci` will exit with an error, instead of updating the package lock.
* `npm ci` can only install entire projects at a time: individual dependencies cannot be added with this command.
* If a `node_modules` is already present, it will be automatically removed before `npm ci` begins its install.
* It will never write to `package.json` or any of the package-locks: installs are essentially frozen.

## SEE ALSO

* npm-install(1)
* npm-package-locks(5)
