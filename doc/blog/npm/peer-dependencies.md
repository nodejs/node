category: npm
title: Peer Dependencies
date: 2013-02-08T00:00:00Z
author: Domenic Denicola
slug: peer-dependencies

<i>Reposted from [Domenic's
blog](http://domenic.me/2013/02/08/peer-dependencies/) with
permission.  Thanks!</i>

npm is awesome as a package manager. In particular, it handles sub-dependencies very well: if my package depends on
`request` version 2 and `some-other-library`, but `some-other-library` depends on `request` version 1, the resulting
dependency graph looks like:

```text
├── request@2.12.0
└─┬ some-other-library@1.2.3
  └── request@1.9.9
```

This is, generally, great: now `some-other-library` has its own copy of `request` v1 that it can use, while not
interfering with my package's v2 copy. Everyone's code works!

## The Problem: Plugins

There's one use case where this falls down, however: *plugins*. A plugin package is meant to be used with another "host"
package, even though it does not always directly *use* the host package. There are many examples of this pattern in the
Node.js package ecosystem already:

- Grunt [plugins](http://gruntjs.com/#plugins-all)
- Chai [plugins](http://chaijs.com/plugins)
- LevelUP [plugins](https://github.com/rvagg/node-levelup/wiki/Modules)
- Express [middleware](http://expressjs.com/api.html#middleware)
- Winston [transports](https://github.com/flatiron/winston/blob/master/docs/transports.md)

Even if you're not familiar with any of those use cases, surely you recall "jQuery plugins" from back when you were a
client-side developer: little `<script>`s you would drop into your page that would attach things to `jQuery.prototype`
for your later convenience.

In essence, plugins are designed to be used with host packages. But more importantly, they're designed to be used with
*particular versions* of host packages. For example, versions 1.x and 2.x of my `chai-as-promised` plugin work with
`chai` version 0.5, whereas versions 3.x work with `chai` 1.x. Or, in the faster-paced and less-semver–friendly world of
Grunt plugins, version 0.3.1 of `grunt-contrib-stylus` works with `grunt` 0.4.0rc4, but breaks when used with `grunt`
0.4.0rc5 due to removed APIs.

As a package manager, a large part of npm's job when installing your dependencies is managing their versions. But its
usual model, with a `"dependencies"` hash in `package.json`, clearly falls down for plugins. Most plugins never actually
depend on their host package, i.e. grunt plugins never do `require("grunt")`, so even if plugins did put down their host
package as a dependency, the downloaded copy would never be used. So we'd be back to square one, with your application
possibly plugging in the plugin to a host package that it's incompatible with.

Even for plugins that do have such direct dependencies, probably due to the host package supplying utility APIs,
specifying the dependency in the plugin's `package.json` would result in a dependency tree with multiple copies of the
host package—not what you want. For example, let's pretend that `winston-mail` 0.2.3 specified `"winston": "0.5.x"` in
its `"dependencies"` hash, since that's the latest version it was tested against. As an app developer, you want the
latest and greatest stuff, so you look up the latest versions of `winston` and of `winston-mail`, putting them in your
`package.json` as

```json
{
  "dependencies": {
    "winston": "0.6.2",
    "winston-mail": "0.2.3"
  }
}
```

But now, running `npm install` results in the unexpected dependency graph of

```text
├── winston@0.6.2
└─┬ winston-mail@0.2.3
  └── winston@0.5.11
```

I'll leave the subtle failures that come from the plugin using a different Winston API than the main application to
your imagination.

## The Solution: Peer Dependencies

What we need is a way of expressing these "dependencies" between plugins and their host package. Some way of saying, "I
only work when plugged in to version 1.2.x of my host package, so if you install me, be sure that it's alongside a
compatible host." We call this relationship a *peer dependency*.

The peer dependency idea has been kicked around for [literally](https://github.com/isaacs/npm/issues/930)
[years](https://github.com/isaacs/npm/issues/1400). After
[volunteering](https://github.com/isaacs/npm/issues/1400#issuecomment-5932027) to get this done "over the weekend" nine
months ago, I finally found a free weekend, and now peer dependencies are in npm!

Specifically, they were introduced in a rudimentary form in npm 1.2.0, and refined over the next few releases into
something I'm actually happy with. Today Isaac packaged up npm 1.2.10 into
[Node.js 0.8.19](http://blog.nodejs.org/2013/02/06/node-v0-8-19-stable/), so if you've installed the latest version of
Node, you should be ready to use peer dependencies!

As proof, I present you the results of trying to install [`jitsu`](https://npmjs.org/package/jitsu) 0.11.6 with npm
1.2.10:

```text
npm ERR! peerinvalid The package flatiron does not satisfy its siblings' peerDependencies requirements!
npm ERR! peerinvalid Peer flatiron-cli-config@0.1.3 wants flatiron@~0.1.9
npm ERR! peerinvalid Peer flatiron-cli-users@0.1.4 wants flatiron@~0.3.0
```

As you can see, `jitsu` depends on two Flatiron-related packages, which themselves peer-depend on conflicting versions
of Flatiron. Good thing npm was around to help us figure out this conflict, so it could be fixed in version 0.11.7!

## Using Peer Dependencies

Peer dependencies are pretty simple to use. When writing a plugin, figure out what version of the host package you
peer-depend on, and add it to your `package.json`:

```json
{
  "name": "chai-as-promised",
  "peerDependencies": {
    "chai": "1.x"
  }
}
```

Now, when installing `chai-as-promised`, the `chai` package will come along with it. And if later you try to install
another Chai plugin that only works with 0.x versions of Chai, you'll get an error. Nice!

One piece of advice: peer dependency requirements, unlike those for regular dependencies, *should be lenient*. You
should not lock your peer dependencies down to specific patch versions. It would be really annoying if one Chai plugin
peer-depended on Chai 1.4.1, while another depended on Chai 1.5.0, simply because the authors were lazy and didn't spend
the time figuring out the actual minimum version of Chai they are compatible with.

The best way to determine what your peer dependency requirements should be is to actually follow
[semver](http://semver.org/). Assume that only changes in the host package's major version will break your plugin. Thus,
if you've worked with every 1.x version of the host package, use `"~1.0"` or `"1.x"` to express this. If you depend on
features introduced in 1.5.2, use `">= 1.5.2 < 2"`.

Now go forth, and peer depend!
