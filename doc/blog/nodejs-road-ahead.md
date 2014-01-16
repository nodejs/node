title: Node.js and the Road Ahead
date: Thu Jan 16 15:00:00 PST 2014
author: Timothy J Fontaine
slug: nodejs-road-ahead

As the new project lead for Node.js I am excited for our future, and want to
give you an update on where we are.

One of Node's major goals is to provide a small core, one that provides the
right amount of surface area for consumers to achieve and innovate, without
Node itself getting in the way. That ethos is alive and well, we're going to
continue to provide a small, simple, and stable set of APIs that facilitate the
amazing uses the community finds for Node. We're going to keep providing
backward compatible APIs, so code you write today will continue to work on
future versions of Node. And of course, performance tuning and bug fixing will
always be an important part of every release cycle.

The release of Node v0.12 is imminent, and a lot of significant work has gone
into this release. There's streams3, a better keep alive agent for http, the vm
module is now based on contextify, and significant performance work done in
core features (Buffers, TLS, streams). We have a few APIs that are still being
ironed out before we can feature freeze and branch (execSync, AsyncListeners,
user definable instrumentation). We are definitely in the home stretch.

But Node is far from done. In the short term there will be new releases of v8
that we'll need to track, as well as integrating the new ABI stable C module
interface. There are interesting language features that we can use to extend
Node APIs (extend not replace). We need to write more tooling, we need to
expose more interfaces to further enable innovation. We can explore
functionality to embed Node in your existing project.

The list can go on and on. Yet, Node is larger than the software itself. Node
is also the community, the businesses, the ecosystems, and their related
events. With that in mind there are things we can work to improve. 

The core team will be improving its procedures such that we can quickly and
efficiently communicate with you. We want to provide high quality and timely
responses to issues, describe our development roadmap, as well as provide our
progress during each release cycle. We know you're interested in our plans for
Node, and it's important we're able to provide that information. Communication
should be bidirectional: we want to continue to receive feedback about how
you're using Node, and what your pain points are.

After the release of v0.12 we will facilitate the community to contribute and
curate content for nodejs.org. Allowing the community to continue to invest in
Node will ensure nodejs.org is an excellent starting point and the primary
resource for tutorials, documentation, and materials regarding Node. We have an
awesome and engaged community, and they're paramount to our success. 

I'm excited for Node's future, to see new and interesting use cases, and to
continue to help businesses scale and innovate with Node. We have a lot we can
accomplish together, and I look forward to seeing those results.
