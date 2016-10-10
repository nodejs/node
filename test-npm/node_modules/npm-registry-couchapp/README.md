# npm-registry-couchapp

[![Build Status](https://img.shields.io/travis/npm/npm-registry-couchapp/master.svg)](https://travis-ci.org/npm/npm-registry-couchapp)

The design doc for The npm Registry CouchApp

# Installing

You need CouchDB version 1.4.0 or higher.  1.5.0 or higher is best.

Once you have CouchDB installed, create a new database:

    curl -X PUT http://localhost:5984/registry

You'll need the following entries added in your `local.ini`

```ini
[couch_httpd_auth]
public_fields = appdotnet, avatar, avatarMedium, avatarLarge, date, email, fields, freenode, fullname, github, homepage, name, roles, twitter, type, _id, _rev
users_db_public = true

[httpd]
secure_rewrites = false

[couchdb]
delayed_commits = false
```

Clone the repository if you haven't already, and cd into it:

    git clone git://github.com/npm/npm-registry-couchapp
    cd npm-registry-couchapp

Now install the stuff:

    npm install

Sync the ddoc to `_design/scratch`

    npm start \
      --npm-registry-couchapp:couch=http://admin:password@localhost:5984/registry

Next, make sure that views are loaded:

    npm run load \
      --npm-registry-couchapp:couch=http://admin:password@localhost:5984/registry

And finally, copy the ddoc from `_design/scratch` to `_design/app`

    npm run copy \
      --npm-registry-couchapp:couch=http://admin:password@localhost:5984/registry

Of course, you can avoid the command-line flag by setting it in your
~/.npmrc file:

    _npm-registry-couchapp:couch=http://admin:password@localhost:5984/registry

The `_` prevents any other packages from seeing the setting (with a
password) in their environment when npm runs scripts for those other
packages.

# Replicating the Registry

To replicate the registry **without attachments**, you can point your
CouchDB replicator at <https://skimdb.npmjs.com/registry>.  Note that
attachments for public packages will still be loaded from the public
location, but anything you publish into your private registry will
stay private.

To replicate the registry **with attachments**, consider using
[npm-fullfat-registry](https://npmjs.org/npm-fullfat-registry).
The fullfatdb CouchDB instance is
[deprecated](http://blog.npmjs.org/post/83774616862/deprecating-fullfatdb).

# Using the registry with the npm client

With the setup so far, you can point the npm client at the registry by
putting this in your ~/.npmrc file:

    registry = http://localhost:5984/registry/_design/app/_rewrite

You can also set the npm registry config property like:

    npm config set \
      registry=http://localhost:5984/registry/_design/app/_rewrite

Or you can simple override the registry config on each call:

    npm \
      --registry=http://localhost:5984/registry/_design/app/_rewrite \
      install <package>

# Optional: top-of-host urls

To be snazzier, add a vhost config:

    [vhosts]
    registry.mydomain.com:5984 = /registry/_design/app/_rewrite

Where `registry.mydomain.com` is the hostname where you're running the
thing, and `5984` is the port that CouchDB is running on. If you're
running on port 80, then omit the port altogether.

Then for example you can reference the repository like so:

    npm config set registry http://registry.mydomain.com:5984
