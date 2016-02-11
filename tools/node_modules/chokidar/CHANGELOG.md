# Chokidar 1.4.2 (Dec 30, 2015)
* Now correctly emitting `stats` with `awaitWriteFinish` option.

# Chokidar 1.4.1 (Dec 9, 2015)
* The watcher could now be correctly subclassed with ES6 class syntax.

# Chokidar 1.4.0 (3 December 2015)
* Add `.getWatched()` method, exposing all file system entries being watched
* Apply `awaitWriteFinish` methodology to `change` events (in addition to `add`)
* Fix handling of symlinks within glob paths (#293)
* Fix `addDir` and `unlinkDir` events under globs (#337, #401)
* Fix issues with `.unwatch()` (#374, #403)

# Chokidar 1.3.0 (18 November 2015)
* Improve `awaitWriteFinish` option behavior
* Fix some `cwd` option behavior on Windows
* `awaitWriteFinish` and `cwd` are now compatible
* Fix some race conditions.
* #379: Recreating deleted directory doesn't trigger event
* When adding a previously-deleted file, emit 'add', not 'change'

# Chokidar 1.2.0 (1 October 2015)
* Allow nested arrays of paths to be provided to `.watch()` and `.add()`
* Add `awaitWriteFinish` option

# Chokidar 1.1.0 (23 September 2015)
* Dependency updates including fsevents@1.0.0, improving installation

# Chokidar 1.0.6 (18 September 2015)
* Fix issue with `.unwatch()` method and relative paths

# Chokidar 1.0.5 (20 July 2015)
* Fix regression with regexes/fns using in `ignored`

# Chokidar 1.0.4 (15 July 2015)
* Fix bug with `ignored` files/globs while `cwd` option is set

# Chokidar 1.0.3 (4 June 2015)
* Fix race issue with `alwaysStat` option and removed files

# Chokidar 1.0.2 (30 May 2015)
* Fix bug with absolute paths and ENAMETOOLONG error

# Chokidar 1.0.1 (8 April 2015)
* Fix bug with `.close()` method in `fs.watch` mode with `persistent: false` option

# Chokidar 1.0.0 (7 April 2015)
* Glob support! Use globs in `watch`, `add`, and `unwatch` methods
* Comprehensive symlink support
* New `unwatch` method to turn off watching of previously watched paths
* More flexible `ignored` option allowing regex, function, glob, or array courtesy of [anymatch](https://github.com/es128/anymatch)
* New `cwd` option to set base dir from which relative paths are derived
* New `depth` option for limiting recursion
* New `alwaysStat` option to ensure [`fs.Stats`](https://nodejs.org/api/fs.html#fs_class_fs_stats) gets passed with every add/change event
* New `ready` event emitted when initial fs tree scan is done and watcher is ready for changes
* New `raw` event exposing data and events from the lower-level watch modules
* New `followSymlinks` option to impact whether symlinks' targets or the symlink files themselves are watched
* New `atomic` option for normalizing artifacts from text editors that use atomic write methods
* Ensured watcher's stability with lots of bugfixes.

# Chokidar 0.12.6 (6 January 2015)
* Fix bug which breaks `persistent: false` mode when change events occur

# Chokidar 0.12.5 (17 December 2014)
* Fix bug with matching parent path detection for fsevents instance sharing
* Fix bug with ignored watch path in nodefs modes

# Chokidar 0.12.4 (14 December 2014)
* Fix bug in `fs.watch` mode that caused watcher to leak into `cwd`
* Fix bug preventing ready event when there are symlinks to ignored paths

# Chokidar 0.12.3 (13 December 2014)
* Fix handling of special files such as named pipes and sockets

# Chokidar 0.12.2 (12 December 2014)
* Fix recursive symlink handling and some other path resolution problems

# Chokidar 0.12.1 (10 December 2014)
* Fix a case where file symlinks were not followed properly

# Chokidar 0.12.0 (8 December 2014)
* Symlink support
  * Add `followSymlinks` option, which defaults to `true`
* Change default watch mode on Linux to non-polling `fs.watch`
* Add `atomic` option to normalize events from editors using atomic writes
  * Particularly Vim and Sublime
* Add `raw` event which exposes data from the underlying watch method

# Chokidar 0.11.1 (19 November 2014)
* Fix a bug where an error is thrown when `fs.watch` instantiation fails

# Chokidar 0.11.0 (16 November 2014)
* Add a `ready` event, which is emitted after initial file scan completes
* Fix issue with options keys passed in defined as `undefined`
* Rename some internal `FSWatcher` properties to indicate they're private

# Chokidar 0.10.9 (15 November 2014)
* Fix some leftover issues from adding watcher reuse

# Chokidar 0.10.8 (14 November 2014)
* Remove accidentally committed/published `console.log` statement.
* Sry 'bout that :crying_cat_face:

# Chokidar 0.10.7 (14 November 2014)
* Apply watcher reuse methodology to `fs.watch` and `fs.watchFile` as well

# Chokidar 0.10.6 (12 November 2014)
* More efficient creation/reuse of FSEvents instances to avoid system limits
* Reduce simultaneous FSEvents instances allowed in a process
* Handle errors thrown by `fs.watch` upon invocation

# Chokidar 0.10.5 (6 November 2014)
* Limit number of simultaneous FSEvents instances (fall back to other methods)
* Prevent some cases of EMFILE errors during initialization
* Fix ignored files emitting events in some fsevents-mode circumstances

# Chokidar 0.10.4 (5 November 2014)
* Bump fsevents dependency to ~0.3.1
  * Should resolve build warnings and `npm rebuild` on non-Macs

# Chokidar 0.10.3 (28 October 2014)
* Fix removed dir emitting as `unlink` instead of `unlinkDir`
* Fix issues with file changing to dir or vice versa (gh-165)
* Fix handling of `ignored` option in fsevents mode

# Chokidar 0.10.2 (23 October 2014)
* Improve individual file watching
* Fix fsevents keeping process alive when `persistent: false`

# Chokidar 0.10.1 (19 October 2014)
* Improve handling of text editor atomic writes

# Chokidar 0.10.0 (18 October 2014)
* Many stability and consistency improvements
* Resolve many cases of duplicate or wrong events
* Correct for fsevents inconsistencies
* Standardize handling of errors and relative paths
* Fix issues with watching `./`

# Chokidar 0.9.0 (25 September 2014)
* Updated fsevents to 0.3
* Update per-system defaults
* Fix issues with closing chokidar instance
* Fix duplicate change events on win32

# Chokidar 0.8.2 (26 March 2014)
* Fixed npm issues related to fsevents dep.
* Updated fsevents to 0.2.

# Chokidar 0.8.1 (16 December 2013)
* Optional deps are now truly optional on windows and
  linux.
* Rewritten in JS, again.
* Fixed some FSEvents-related bugs.

# Chokidar 0.8.0 (29 November 2013)
* Added ultra-fast low-CPU OS X file watching with FSEvents.
  It is enabled by default.
* Added `addDir` and `unlinkDir` events.
* Polling is now disabled by default on all platforms.

# Chokidar 0.7.1 (18 November 2013)
* `Watcher#close` now also removes all event listeners.

# Chokidar 0.7.0 (22 October 2013)
* When `options.ignored` is two-argument function, it will
  also be called after stating the FS, with `stats` argument.
* `unlink` is no longer emitted on directories.

# Chokidar 0.6.3 (12 August 2013)
* Added `usePolling` option (default: `true`).
  When `false`, chokidar will use `fs.watch` as backend.
  `fs.watch` is much faster, but not like super reliable.

# Chokidar 0.6.2 (19 March 2013)
* Fixed watching initially empty directories with `ignoreInitial` option.

# Chokidar 0.6.1 (19 March 2013)
* Added node.js 0.10 support.

# Chokidar 0.6.0 (10 March 2013)
* File attributes (stat()) are now passed to `add` and `change` events
  as second arguments.
* Changed default polling interval for binary files to 300ms.

# Chokidar 0.5.3 (13 January 2013)
* Removed emitting of `change` events before `unlink`.

# Chokidar 0.5.2 (13 January 2013)
* Removed postinstall script to prevent various npm bugs.

# Chokidar 0.5.1 (6 January 2013)
* When starting to watch non-existing paths, chokidar will no longer throw
ENOENT error.
* Fixed bug with absolute path.

# Chokidar 0.5.0 (9 December 2012)
* Added a bunch of new options:
    * `ignoreInitial` that allows to ignore initial `add` events.
    * `ignorePermissionErrors` that allows to ignore ENOENT etc perm errors.
    * `interval` and `binaryInterval` that allow to change default
    fs polling intervals.

# Chokidar 0.4.0 (26 July 2012)
* Added `all` event that receives two args (event name and path) that
combines `add`, `change` and `unlink` events.
* Switched to `fs.watchFile` on node.js 0.8 on windows.
* Files are now correctly unwatched after unlink.

# Chokidar 0.3.0 (24 June 2012)
* `unlink` event are no longer emitted for directories, for consistency
with `add`.

# Chokidar 0.2.6 (8 June 2012)
* Prevented creating of duplicate 'add' events.

# Chokidar 0.2.5 (8 June 2012)
* Fixed a bug when new files in new directories hadn't been added.

# Chokidar 0.2.4 (7 June 2012)
* Fixed a bug when unlinked files emitted events after unlink.

# Chokidar 0.2.3 (12 May 2012)
* Fixed watching of files on windows.

# Chokidar 0.2.2 (4 May 2012)
* Fixed watcher signature.

# Chokidar 0.2.1 (4 May 2012)
* Fixed invalid API bug when using `watch()`.

# Chokidar 0.2.0 (4 May 2012)
* Rewritten in js.

# Chokidar 0.1.1 (26 April 2012)
* Changed api to `chokidar.watch()`.
* Fixed compilation on windows.

# Chokidar 0.1.0 (20 April 2012)
* Initial release.
