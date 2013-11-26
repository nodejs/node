date: Tue Nov 26 07:14:59 PST 2013
author: Charlie Robbins
title: Keeping The npm Registry Awesome
slug: npm-post-mortem
category: npm

We know the availability and overall health of The npm Registry is paramount to everyone using Node.js as well as the larger JavaScript community and those of your using it for [some][browserify] [awesome][dotc] [projects][npm-rubygems] [and ideas][npm-python]. Between November 4th and November 15th 2013 The npm Registry had several hours of downtime over three distinct time periods:

1. November 4th -- 16:30 to 15:00 UTC
2. November 13th -- 15:00 to 19:30 UTC
3. November 15th -- 15:30 to 18:00 UTC

The root cause of these downtime was insufficient resources: both hardware and human. This is a full post-mortem where we will be look at how npmjs.org works, what went wrong, how we changed the previous architecture of The npm Registry to fix it, as well next steps we are taking to prevent this from happening again.

All of the next steps require additional expenditure from Nodejitsu: both servers and labor. This is why along with this post-mortem we are announcing our [crowdfunding campaign: scalenpm.org](https://scalenpm.org)! Our goal is to raise enough funds so that Nodejitsu can continue to run The npm Registry as a free service for _you, the community._ 

Please take a minute now to donate at [https://scalenpm.org](https://scalenpm.org)! 

## How does npmjs.org work?

There are two distinct components that make up npmjs.org operated by different people:

* **http://registry.npmjs.org**: The main CouchApp (Github: [isaacs/npmjs.org](https://github.com/isaacs/npmjs.org)) that stores both package tarballs and metadata. It is operated by Nodejitsu since we [acquired IrisCouch in May](https://www.nodejitsu.com/company/press/2013/05/22/iriscouch/). The primary system administrator is [Jason Smith](https://github.com/jhs), the current CTO at Nodejitsu, cofounder of IrisCouch, and the System Administrator of registry.npmjs.org since 2011.
* **http://npmjs.org**: The npmjs website that you interact with using a web browser. It is a Node.js program (Github: [isaacs/npm-www](https://github.com/isaacs/npm-www)) maintained and operated by Isaac and running on a Joyent Public Cloud SmartMachine.

Here is a high-level summary of the _old architecture:_

<img width=600 src="https://i.cloudup.com/bapm3fk8Ve-3000x3000.png" alt="old npm architecture">
<div style="text-align:center">
  _Diagram 1. Old npm architecture_
</div>

## What went wrong and how was it fixed?

As illustrated above, before November 13th, 2013, npm operated as a single CouchDB server with regular daily backups. We briefly ran a multi-master CouchDB setup after downtime back in August, but after reports that `npm login` no longer worked correctly we rolled back to a single CouchDB server. On both November 13th and November 15th CouchDB became unresponsive on requests to the `/registry` database while requests to all other databases (e.g. `/public_users`) remained responsive. Although the root cause of the CouchDB failures have yet to be determined given that only requests to `/registry` were slow and/or timed out we suspect it is related to the massive number of attachments stored in the registry.

The incident on November 4th was ultimately resolved by a reboot and resize of the host machine, but when the same symptoms reoccured less than 10 days later additional steps were taken:

1. The [registry was moved to another machine][ops-new-machine] of equal resources to exclude the possibility of a hardware issue.
2. The [registry database itself][ops-compaction] was [compacted][compaction].

When neither of these yielded a solution Jason Smith and I decided to move to a multi-master architecture with continuous replication illustrated below:

<img width=600 src="https://i.cloudup.com/xu1faVCq8p-3000x3000.png" alt="current npm architecture">
<div style="text-align:center">
  _Diagram 2. Current npm architecture -- Red-lines denote continuous replication_
</div>

This _should_ have been the end of our story but unfortunately our supervision logic did not function properly to restart the secondary master on the morning of November 15th. During this time we [moved briefly][ops-single-server] back to a single master architecture. Since then the secondary master has been closely monitored by the entire Nodejitsu operations team to ensure it's continued stability.

## What is being done to prevent future incidents?

The public npm registry simply cannot go down. **Ever.** We gained a lot of operational knowledge about The npm Registry and about CouchDB as a result of these outages. This new knowledge has made clear several steps that we need to take to prevent future downtime:

1. **Always be in multi-master**: The multi-master CouchDB architecture we have setup will scale to more than just two CouchDB servers. _As npm grows we'll be able to add additional capacity!_
2. **Decouple www.npmjs.org and registry.npmjs.org**: Right now www.npmjs.org still depends directly on registry.npmjs.org. We are planning to add an additional replica to the current npm architecture so that Isaac can more easily service requests to www.npmjs.org. That means it won't go down if the registry goes down.
3. **Always have a spare replica**: We need have a hot spare replica running continuous replication from either to swap out when necessary. This is also important as we need to regularly run compaction on each master since the registry is growing ~10GB per week on disk.
4. **Move attachments out of CouchDB**: Work has begun to move the package tarballs out of CouchDB and into [Joyent's Manta service](http://www.joyent.com/products/manta). Additionally, [MaxCDN](http://www.maxcdn.com/) has generously offered to provide CDN services for npm, once the tarballs are moved out of the registry database.  This will help improve delivery speed, while dramatically reducing the file system I/O load on the CouchDB servers.  Work is progressing slowly, because at each stage in the plan, we are making sure that current replication users are minimally impacted.

When these new infrastructure components are in-place The npm Registry will look like this:

<img width=600 src="https://i.cloudup.com/XwrpFNICJ2-3000x3000.png" alt="planned npm architecture">
<div style="text-align:center">
  _Diagram 3. Planned npm architecture -- Red-lines denote continuous replication_
</div>

## You are npm! And we need your help!

The npm Registry has had a 10x year. In November 2012 there were 13.5 million downloads. In October 2013 there were **114.6 million package downloads.** We're honored to have been a part of sustaining this growth for the community and we want to see it continue to grow to a billion package downloads a month and beyond.

_**But we need your help!**_ All of these necessary improvements require more servers, more time from Nodejitsu staff and an overall increase to what we spend maintaining the public npm registry as a free service for the Node.js community.

Please take a minute now to donate at [https://scalenpm.org](https://scalenpm.org)! 

[browserify]: http://browserify.org/
[dotc]: https://github.com/substack/dotc
[npm-rubygems]: http://andrew.ghost.io/emulating-node-js-modules-in-ruby/
[npm-python]: https://twitter.com/__lucas/status/391688082573258753
[ops-new-machine]: https://twitter.com/npmjs/status/400692071377276928
[ops-compaction]: https://twitter.com/npmjs/status/400705715846643712
[compaction]: http://wiki.apache.org/couchdb/Compaction
[ops-single-server]: https://twitter.com/npmjs/status/401384681507016704
