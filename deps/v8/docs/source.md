# Source

**Quick links:** [browse](https://chromium.googlesource.com/v8/v8/) | [browse bleeding edge](https://chromium.googlesource.com/v8/v8/+/master) | [changes](https://chromium.googlesource.com/v8/v8/+log/master).

## Command-Line Access

### Git
See [UsingGit](using_git.md).

### Subversion (deprecated)

Use this command to anonymously check out the up-to-date stable version of the project source code:

> `svn checkout http://v8.googlecode.com/svn/trunk/ v8`

If you plan to contribute to V8 but are not a member, use this command to anonymously check out a read-only version of the development branch:

> `svn checkout http://v8.googlecode.com/svn/branches/bleeding_edge/ v8`

If you're a member of the project, use this command to check out a writable development branch as yourself using HTTPS:

> `svn checkout https://v8.googlecode.com/svn/branches/bleeding_edge/ v8 --username <your username>`

When prompted, enter your generated [googlecode.com](http://code.google.com/hosting/settings) password.

## Source Code Branches

There are several different branches of V8; if you're unsure of which version to get, you most likely want the up-to-date stable version in `trunk/`. Here's an overview of the different branches:

  * The bleeding edge, `branches/bleeding_edge/`, is where active development takes place. If you're considering contributing to V8 this is the branch to get.
  * Under `trunk/` is the "stable edge", which is updated a few times per week. It is a copy of the bleeding edge that has been successfully tested. Use this if you want to be almost up to date and don't want your code to break whenever we accidentally forget to add a file on the bleeding edge. Some of the trunk revisions are tagged with X.Y.Z.T version labels. When we decide which of X.Y.**.** is the "most stable", it becomes the X.Y branch in subversion.
  * If you want a well-tested version that doesn't change except for bugfixes, use one of the versioned branches (e.g. `branches/3.16/` at the time of this writing). Note that usually only the last two branches are actively maintained; any older branches could have unfixed security holes. You may want to follow the V8 version that Chrome is shipping on its stable (or beta) channels, see http://omahaproxy.appspot.com.

## V8 public API compatibility

V8 public API (basically the files under include/ directory) may change over time. New types/methods may be added without breaking existing functionality. When we decide that want to drop some existing class/methods, we first mark it with [V8\_DEPRECATED](https://code.google.com/p/chromium/codesearch#search/&q=V8_DEPRECATED&sq=package:chromium&type=cs) macro which will cause compile time warnings when the deprecated methods are called by the embedder. We keep deprecated method for one branch and then remove it. E.g. if `v8::CpuProfiler::FindCpuProfile` was plain non deprecated in _3.17_ branch, marked as `V8_DEPRECATED` in _3.18_, it may well be removed in _3.19_ branch.


## GUI and IDE Access

This project's Subversion repository may be accessed using many different client programs and plug-ins. See your client's documentation for more information.
