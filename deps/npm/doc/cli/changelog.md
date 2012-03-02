npm-changelog(1) -- Changes
===========================

## HISTORY

### 1.1.3, 1.1.4

* Update request to support HTTPS-over-HTTP proxy tunneling
* Throw on undefined envs in config settings
* Update which to 1.0.5
* Fix windows UNC busyloop in findPrefix
* Bundle nested bundleDependencies properly
* Alias adduser to add-user
* Doc updates  (Christian Howe, Henrik Hodne, Andrew Lunny)
* ignore logfd/outfd streams in makeEnv() (Rod Vagg)
* shrinkwrap: Behave properly with url-installed deps
* install: Support --save with url install targets
* Support installing naked tars or single-file modules from urls etc.
* init: Don't add engines section
* Don't run make clean on rebuild
* Added missing unicode replacement (atomizer)

### 1.1.2

Dave Pacheco (2):
      add "npm shrinkwrap"

Martin Cooper (1):
      Fix #1753 Make a copy of the cached objects we'll modify.

Tim Oxley (1):
      correctly remove readme from default npm view command.

Tyler Green (1):
      fix #2187 set terminal columns to Infinity if 0

isaacs (19):
      update minimatch
      update request
      Experimental: single-file modules
      Fix #2172 Don't remove global mans uninstalling local pkgs
      Add --versions flag to show the version of node as well
      Support --json flag for ls output
      update request to 2.9.151

### 1.1  
* Replace system tar dependency with a JS tar
* Continue to refine

### 1.0  
* Greatly simplified folder structure 
* Install locally (bundle by default) 
* Drastic rearchitecture

### 0.3  
* More correct permission/uid handling when running as root  
* Require node 0.4.0  
* Reduce featureset  
* Packages without "main" modules don't export modules
* Remove support for invalid JSON (since node doesn't support it)

### 0.2  
* First allegedly "stable" release
* Most functionality implemented 
* Used shim files and `name@version` symlinks
* Feature explosion
* Kind of a mess

### 0.1  
* push to beta, and announce  
* Solaris and Cygwin support

### 0.0  
* Lots of sketches and false starts; abandoned a few times
* Core functionality established

## SEE ALSO

* npm(1)
* npm-faq(1)
