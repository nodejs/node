npm-cache(1) -- Manipulates packages cache
==========================================

## SYNOPSIS

    npm cache add <tarball file>
    npm cache add <folder>
    npm cache add <tarball url>
    npm cache add <name>@<version>

    npm cache ls [<path>]

    npm cache clean [<path>]
    aliases: npm cache clear, npm cache rm

## DESCRIPTION

Used to add, list, or clean the npm cache folder.

* add:
  Add the specified package to the local cache.  This command is primarily
  intended to be used internally by npm, but it can provide a way to
  add data to the local installation cache explicitly.

* ls:
  Show the data in the cache.  Argument is a path to show in the cache
  folder.  Works a bit like the `find` program, but limited by the
  `depth` config.

* clean:
  Delete data out of the cache folder.  If an argument is provided, then
  it specifies a subpath to delete.  If no argument is provided, then
  the entire cache is deleted.

## DETAILS

npm stores cache data in the directory specified in `npm config get cache`.
For each package that is added to the cache, three pieces of information are
stored in `{cache}/{name}/{version}`:

* .../package/package.json:
  The package.json file, as npm sees it.
* .../package.tgz:
  The tarball for that version.

Additionally, whenever a registry request is made, a `.cache.json` file
is placed at the corresponding URI, to store the ETag and the requested
data.  This is stored in `{cache}/{hostname}/{path}/.cache.json`.

Commands that make non-essential registry requests (such as `search` and
`view`, or the completion scripts) generally specify a minimum timeout.
If the `.cache.json` file is younger than the specified timeout, then
they do not make an HTTP request to the registry.

## CONFIGURATION

### cache

Default: `~/.npm` on Posix, or `%AppData%/npm-cache` on Windows.

The root cache folder.

## SEE ALSO

* npm-folders(5)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-install(1)
* npm-publish(1)
* npm-pack(1)
