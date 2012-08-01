title: ldapjs: A reprise of LDAP
author: mcavage
date: Thu Sep 08 2011 14:25:43 GMT-0700 (PDT)
status: publish
category: Uncategorized
slug: ldapjs-a-reprise-of-ldap

This post has been about 10 years in the making. My first job out of college was at IBM working on the <a title="Tivoli Directory Server" href="http://www-01.ibm.com/software/tivoli/products/directory-server/">Tivoli Directory Server</a>, and at the time I had a preconceived notion that working on anything related to Internet RFCs was about as hot as you could get. I spent a lot of time back then getting "down and dirty" with everything about LDAP: the protocol, performance, storage engines, indexing and querying, caching, customer use cases and patterns, general network server patterns, etc. Basically, I soaked up as much as I possibly could while I was there. On top of that, I listened to all the "gray beards" tell me about the history of LDAP, which was a bizarre marriage of telecommunications conglomerates and graduate students. The point of this blog post is to give you a crash course in LDAP, and explain what makes <a title="ldapjs" href="http://ldapjs.org">ldapjs</a> different. Allow me to be the gray beard for a bit...
<h2>What is LDAP and where did it come from?</h2>

Directory services were largely pioneered by the telecommunications companies (e.g., AT&amp;T) to allow fast information retrieval of all the crap you'd expect would be in a telephone book and directory. That is, given a name, or an address, or an area code, or a number, or a foo support looking up customer records, billing information, routing information, etc. The efforts of several telcos came to exist in the <a title="X.500" href="http://en.wikipedia.org/wiki/X.500">X.500</a> standard(s). An X.500 directory is one of the most complicated beasts you can possibly imagine, but on a high note, there's
probably not a thing you can imagine in a directory service that wasn't thought of in there. It is literally the kitchen sink. Oh, and it doesn't run over IP (it's <em>actually</em> on the <a title="OSI Model" href="http://en.wikipedia.org/wiki/OSI_model">OSI</a> model).

Several years after X.500 had been deployed (at telcos, academic institutions, etc.), it became clear that the Internet was "for real." <a title="LDAP" href="http://en.wikipedia.org/wiki/Lightweight_Directory_Access_Protocol">LDAP</a>, the "Lightweight Directory Access Protocol," was invented to act purely as an IP-accessible gateway to an X.500 directory.

At some point in the early 90's, a <a title="Tim Howes" href="http://en.wikipedia.org/wiki/Tim_Howes">graduate student</a> at the University of Michigan (with some help) cooked up the "grandfather" implementation of the LDAP protocol, which wasn't actually a "gateway," but rather a stand-alone implementation of LDAP. Said implementation, like many things at the time, was a process-per-connection concurrency model, and had "backends" (aka storage engine) for the file system and the Unix DB API. At some point the <a title="Berkeley Database" href="http://www.oracle.com/technetwork/database/berkeleydb/index.html">Berkeley Database </a>(BDB) was put in, and still remains the de facto storage engine for most LDAP directories.

Ok, so some a graduate student at UM wrote an LDAP server that wasn't a gateway. So what? Well, that UM code base turns out to be the thing that pretty much every vendor did a source license for. Those graduate students went off to Netscape later in the 90's, and largely dominated the market of LDAP middleware until <a title="Active Directory" href="http://en.wikipedia.org/wiki/Active_Directory">Active Directory</a> came along many years later (as far as I know, Active Directory is "from scratch", since while it's "almost" LDAP, it's different in a lot of ways). That Netscape code base was further bought and sold over the years to iPlanet, Sun Microsystems, and Red Hat (I'm probably missing somebody in that chain). It now lives in the Fedora umbrella as '<a title="389 Directory Server" href="http://directory.fedoraproject.org/">389 Directory Server</a>.' Probably the most popular fork of that code base now is <a title="OpenLDAP" href="http://www.openldap.org/">OpenLDAP</a>.

IBM did the same thing, and the Directory Server I worked on was a fork of the UM code too, but it heavily diverged from the Netscape branches. The divergence was primarily due to: (1) backing to DB2 as opposed to BDB, and (2) needing to run on IBM's big iron like OS/400 and Z series mainframes.

Macro point is that there have actually been very few "fresh" implementations of LDAP, and it gets a pretty bad reputation because at the end of the day you've got 20 years of "bolt-ons" to grad student code. Oh, and it was born out of ginormous telcos, so of course the protocol is overly complex.

That said, while there certainly is some wacky stuff in the LDAP protocol itself, it really suffered from poor and buggy implementations more than the fact that LDAP itself was fundamentally flawed. As <a title="Engine Yard LDAP" href="http://www.engineyard.com/blog/2009/ldap-directories-the-forgotten-nosql/">engine yard pointed out a few years back</a>, you can think of LDAP as the original NoSQL store.
<h2>LDAP: The Good Parts</h2>

So what's awesome about LDAP? Since it's a directory system it maintains a hierarchy of your data, which as an information management pattern aligns
with _a lot_ of use case (the quintessential example is white pages for people in your company, but subscriptions to SaaS applications, "host groups"
for tracking machines/instances, physical goods tracking, etc., all have use cases that fit that organization scheme). For example, presumably at your job
you have a "reporting chain." Let's say a given record in LDAP (I'll use myself as a guinea pig here) looks like:
<pre>    firstName: Mark
    lastName: Cavage
    city: Seattle
    uid: markc
    state: Washington
    mail: mcavagegmailcom
    phone: (206) 555-1212
    title: Software Engineer
    department: 123456
    objectclass: joyentPerson</pre>
The record for me would live under the tree of engineers I report to (and as an example some other popular engineers under said vice president) would look like:
<pre>                   uid=david
                    /
               uid=bryan
            /      |      \
      uid=markc  uid=ryah  uid=isaacs</pre>
Ok, so we've got a tree. It's not tremendously different from your filesystem, but how do we find people? LDAP has a rich search filter syntax that makes a lot of sense for key/value data (far more than tacking Map Reduce jobs on does, imo), and all search queries take a "start point" in the tree. Here's an example: let's say I wanted to find all "Software Engineers" in the entire company, a filter would look like:
<pre>     (title="Software Engineer")</pre>
And I'd just start my search from 'uid=david' in the example above. Let's say I wanted to find all software engineers who worked in Seattle:
<pre>     (&amp;(title="Software Engineer")(city=Seattle))</pre>
I could keep going, but the gist is that LDAP has "full" boolean predicate logic, wildcard filters, etc. It's really rich.

Oh, and on top of the technical merits, better or worse, it's an established standard for both administrators and applications (i.e., most "shipped" intranet software has either a local user repository or the ability to leverage an LDAP server somewhere). So there's a lot of compelling reasons to look at leveraging LDAP.
<h2>ldapjs: Why do I care?</h2>

As I said earlier, I spent a lot of time at IBM observing how customers used LDAP, and the real items I took away from that experience were:
<ul>
	<li>LDAP implementations have suffered a lot from never having been designed from the ground up for a large number of concurrent connections with asynchronous operations.</li>
	<li>There are use cases for LDAP that just don't always fit the traditional "here's my server and storage engine" model. A lot of simple customer use cases wanted an LDAP access point, but not be forced into taking the heavy backends that came with it (they wanted the original gateway model!). There was an entire "sub" industry for this known as "<a title="Metadirectory" href="http://en.wikipedia.org/wiki/Metadirectory">meta directories</a>" back in the late 90's and early 2000's.</li>
	<li>Replication was always a sticking point. LDAP vendors all tried to offer a big multi-master, multi-site replication model. It was a lot of "bolt-on" complexity, done before the <a title="CAP Theorem" href="http://en.wikipedia.org/wiki/CAP_theorem">CAP theorem</a> was written, and certainly before it was accepted as "truth."</li>
	<li>Nobody uses all of the protocol. In fact, 20% of the features solve 80% of the use cases (I'm making that number up, but you get the idea).</li>
</ul>

For all the good parts of LDAP, those are really damned big failing points, and even I eventually abandoned LDAP for the greener pastures of NoSQL somewhere
along the way. But it always nagged at me that LDAP didn't get it's due because of a lot of implementation problems (to be clear, if I could, I'd change some
aspects of the protocol itself too, but that's a lot harder).

Well, in the last year, I went to work for <a title="Joyent" href="http://www.joyent.com/">Joyent</a>, and like everyone else, we have several use problems that are classic directory service problems. If you break down the list I outlined above:
<ul>
	<li><strong>Connection-oriented and asynchronous:</strong> Holy smokes batman, <a title="node.js" href="http://nodejs.org/">node.js</a> is a completely kick-ass event-driven asynchronous server platform that manages connections like a boss. Check!</li>
	<li><strong>Lots of use cases:</strong> Yeah, we've got some. Man, the <a title="sinatra" href="http://www.sinatrarb.com/">sinatra</a>/<a title="express" href="http://expressjs.com/">express</a> paradigm is so easy to slap over anything. How about we just do that and leave as many use cases open as we can. Check!</li>
	<li><strong>Replication is hard. CAP is right:</strong> There are a lot of distributed databases out vying to solve exactly this problem. At Joyent we went with <a title="Riak" href="http://www.basho.com/">Riak</a>. Check!</li>
	<li><strong>Don't need all of the protocol:</strong> I'm lazy. Let's just skip the stupid things most people don't need. Check!</li>
</ul>

So that's the crux of ldapjs right there. Giving you the ability to put LDAP back into your application while nailing those 4 fundamental problems that plague most existing LDAP deployments.

The obvious question is how it turned out, and the answer is, honestly, better than I thought it would. When I set out to do this, I actually assumed I'd be shipping a much smaller percentage of the RFC than is there. There's actually about 95% of the core RFC implemented. I wasn't sure if the marriage of this protocol to node/JavaScript would work out, but if you've used express ever, this should be _really_ familiar. And I tried to make it as natural as possible to use "pure" JavaScript objects, rather than requiring the developer to understand <a title="ASN.1" href="http://en.wikipedia.org/wiki/Abstract_Syntax_Notation_One">ASN.1</a> (the binary wire protocol) or the<a title="RFC 4510" href="http://tools.ietf.org/html/rfc4510"> LDAP RFC</a> in detail (this one mostly worked out; ldap_modify is still kind of a PITA).

Within 24 hours of releasing ldapjs on <a title="twitter" href="http://twitter.com/#!/mcavage/status/106767571012952064">Twitter</a>, there was an <a title="github ldapjs address book" href="https://gist.github.com/1173999">implementation of an address book</a> that works with Thunderbird/Evolution, by the end of that weekend there was some <a href="http://i.imgur.com/uR16U.png">slick integration with CouchDB</a>, and ldapjs even got used in one of the <a href="http://twitter.com/#!/jheusala/status/108977708649811970">node knockout apps</a>. Off to a pretty good start!

<h2>The Road Ahead</h2>

Hopefully you've been motivated to learn a little bit more about LDAP and try out <a href="http://ldapjs.org">ldapjs</a>. The best place to start is probably the <a title="ldapjs guide" href="http://ldapjs.org/guide.html">guide</a>. After that you'll probably need to pick up a book from <a href="http://www.amazon.com/Understanding-Deploying-LDAP-Directory-Services/dp/0672323168">back in the day</a>. ldapjs itself is still in its infancy; there's quite a bit of room to add some slick client-side logic (e.g., connection pools, automatic reconnects), easy to use schema validation, backends, etc. By the time this post is live, there will be experimental <a href="http://en.wikipedia.org/wiki/DTrace">dtrace</a> support if you're running on Mac OS X or preferably Joyent's <a href="http://smartos.org/">SmartOS</a> (shameless plug). And that nagging percentage of the protocol I didn't do will get filled in over time I suspect. If you've got an interest in any of this, send me some pull requests, but most importantly, I just want to see LDAP not just be a skeleton in the closet and get used in places where you should be using it. So get out there and write you some LDAP.
