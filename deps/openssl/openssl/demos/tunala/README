This is intended to be an example of a state-machine driven SSL application. It
acts as an SSL tunneler (functioning as either the server or client half,
depending on command-line arguments). *PLEASE* read the comments in tunala.h
before you treat this stuff as anything more than a curiosity - YOU HAVE BEEN
WARNED!! There, that's the draconian bit out of the way ...


Why "tunala"??
--------------

I thought I asked you to read tunala.h?? :-)


Show me
-------

If you want to simply see it running, skip to the end and see some example
command-line arguments to demonstrate with.


Where to look and what to do?
-----------------------------

The code is split up roughly coinciding with the detaching of an "abstract" SSL
state machine (which is the purpose of all this) and its surrounding application
specifics. This is primarily to make it possible for me to know when I could cut
corners and when I needed to be rigorous (or at least maintain the pretense as
such :-).

Network stuff:

Basically, the network part of all this is what is supposed to be abstracted out
of the way. The intention is to illustrate one way to stick OpenSSL's mechanisms
inside a little memory-driven sandbox and operate it like a pure state-machine.
So, the network code is inside both ip.c (general utility functions and gory
IPv4 details) and tunala.c itself, which takes care of application specifics
like the main select() loop. The connectivity between the specifics of this
application (TCP/IP tunneling and the associated network code) and the
underlying abstract SSL state machine stuff is through the use of the "buffer_t"
type, declared in tunala.h and implemented in buffer.c.

State machine:

Which leaves us, generally speaking, with the abstract "state machine" code left
over and this is sitting inside sm.c, with declarations inside tunala.h. As can
be seen by the definition of the state_machine_t structure and the associated
functions to manipulate it, there are the 3 OpenSSL "handles" plus 4 buffer_t
structures dealing with IO on both the encrypted and unencrypted sides ("dirty"
and "clean" respectively). The "SSL" handle is what facilitates the reading and
writing of the unencrypted (tunneled) data. The two "BIO" handles act as the
read and write channels for encrypted tunnel traffic - in other applications
these are often socket BIOs so that the OpenSSL framework operates with the
network layer directly. In this example, those two BIOs are memory BIOs
(BIO_s_mem()) so that the sending and receiving of the tunnel traffic stays
within the state-machine, and we can handle where this gets send to (or read
from) ourselves.


Why?
----

If you take a look at the "state_machine_t" section of tunala.h and the code in
sm.c, you will notice that nothing related to the concept of 'transport' is
involved. The binding to TCP/IP networking occurs in tunala.c, specifically
within the "tunala_item_t" structure that associates a state_machine_t object
with 4 file-descriptors. The way to best see where the bridge between the
outside world (TCP/IP reads, writes, select()s, file-descriptors, etc) and the
state machine is, is to examine the "tunala_item_io()" function in tunala.c.
This is currently around lines 641-732 but of course could be subject to change.


And...?
-------

Well, although that function is around 90 lines of code, it could easily have
been a lot less only I was trying to address an easily missed "gotcha" (item (2)
below). The main() code that drives the select/accept/IO loop initialises new
tunala_item_t structures when connections arrive, and works out which
file-descriptors go where depending on whether we're an SSL client or server
(client --> accepted connection is clean and proxied is dirty, server -->
accepted connection is dirty and proxied is clean). What that tunala_item_io()
function is attempting to do is 2 things;

  (1) Perform all reads and writes on the network directly into the
      state_machine_t's buffers (based on a previous select() result), and only
      then allow the abstact state_machine_t to "churn()" using those buffers.
      This will cause the SSL machine to consume as much input data from the two
      "IN" buffers as possible, and generate as much output data into the two
      "OUT" buffers as possible. Back up in the main() function, the next main
      loop loop will examine these output buffers and select() for writability
      on the corresponding sockets if the buffers are non-empty.

  (2) Handle the complicated tunneling-specific issue of cascading "close"s.
      This is the reason for most of the complexity in the logic - if one side
      of the tunnel is closed, you can't simply close the other side and throw
      away the whole thing - (a) there may still be outgoing data on the other
      side of the tunnel that hasn't been sent yet, (b) the close (or things
      happening during the close) may cause more data to be generated that needs
      sending on the other side. Of course, this logic is complicated yet futher
      by the fact that it's different depending on which side closes first :-)
      state_machine_close_clean() will indicate to the state machine that the
      unencrypted side of the tunnel has closed, so any existing outgoing data
      needs to be flushed, and the SSL stream needs to be closed down using the
      appropriate shutdown sequence. state_machine_close_dirty() is simpler
      because it indicates that the SSL stream has been disconnected, so all
      that remains before closing the other side is to flush out anything that
      remains and wait for it to all be sent.

Anyway, with those things in mind, the code should be a little easier to follow
in terms of "what is *this* bit supposed to achieve??!!".


How might this help?
--------------------

Well, the reason I wrote this is that there seemed to be rather a flood of
questions of late on the openssl-dev and openssl-users lists about getting this
whole IO logic thing sorted out, particularly by those who were trying to either
use non-blocking IO, or wanted SSL in an environment where "something else" was
handling the network already and they needed to operate in memory only. This
code is loosely based on some other stuff I've been working on, although that
stuff is far more complete, far more dependant on a whole slew of other
network/framework code I don't want to incorporate here, and far harder to look
at for 5 minutes and follow where everything is going. I will be trying over
time to suck in a few things from that into this demo in the hopes it might be
more useful, and maybe to even make this demo usable as a utility of its own.
Possible things include:

  * controlling multiple processes/threads - this can be used to combat
    latencies and get passed file-descriptor limits on some systems, and it uses
    a "controller" process/thread that maintains IPC links with the
    processes/threads doing the real work.

  * cert verification rules - having some say over which certs get in or out :-)

  * control over SSL protocols and cipher suites

  * A few other things you can already do in s_client and s_server :-)

  * Support (and control over) session resuming, particularly when functioning
    as an SSL client.

If you have a particular environment where this model might work to let you "do
SSL" without having OpenSSL be aware of the transport, then you should find you
could use the state_machine_t structure (or your own variant thereof) and hook
it up to your transport stuff in much the way tunala.c matches it up with those
4 file-descriptors. The state_machine_churn(), state_machine_close_clean(), and
state_machine_close_dirty() functions are the main things to understand - after
that's done, you just have to ensure you're feeding and bleeding the 4
state_machine buffers in a logical fashion. This state_machine loop handles not
only handshakes and normal streaming, but also renegotiates - there's no special
handling required beyond keeping an eye on those 4 buffers and keeping them in
sync with your outer "loop" logic. Ie. if one of the OUT buffers is not empty,
you need to find an opportunity to try and forward its data on. If one of the IN
buffers is not full, you should keep an eye out for data arriving that should be
placed there.

This approach could hopefully also allow you to run the SSL protocol in very
different environments. As an example, you could support encrypted event-driven
IPC where threads/processes pass messages to each other inside an SSL layer;
each IPC-message's payload would be in fact the "dirty" content, and the "clean"
payload coming out of the tunnel at each end would be the real intended message.
Likewise, this could *easily* be made to work across unix domain sockets, or
even entirely different network/comms protocols.

This is also a quick and easy way to do VPN if you (and the remote network's
gateway) support virtual network devices that are encapsulted in a single
network connection, perhaps PPP going through an SSL tunnel?


Suggestions
-----------

Please let me know if you find this useful, or if there's anything wrong or
simply too confusing about it. Patches are also welcome, but please attach a
description of what it changes and why, and "diff -urN" format is preferred.
Mail to geoff@openssl.org should do the trick.


Example
-------

Here is an example of how to use "tunala" ...

First, it's assumed that OpenSSL has already built, and that you are building
inside the ./demos/tunala/ directory. If not - please correct the paths and
flags inside the Makefile. Likewise, if you want to tweak the building, it's
best to try and do so in the makefile (eg. removing the debug flags and adding
optimisation flags).

Secondly, this code has mostly only been tested on Linux. However, some
autoconf/etc support has been added and the code has been compiled on openbsd
and solaris using that.

Thirdly, if you are Win32, you probably need to do some *major* rewriting of
ip.c to stand a hope in hell. Good luck, and please mail me the diff if you do
this, otherwise I will take a look at another time. It can certainly be done,
but it's very non-POSIXy.

See the INSTALL document for details on building.

Now, if you don't have an executable "tunala" compiled, go back to "First,...".
Rinse and repeat.

Inside one console, try typing;

(i)  ./tunala -listen localhost:8080 -proxy localhost:8081 -cacert CA.pem \
              -cert A-client.pem -out_totals -v_peer -v_strict

In another console, type;

(ii) ./tunala -listen localhost:8081 -proxy localhost:23 -cacert CA.pem \
              -cert A-server.pem -server 1 -out_totals -v_peer -v_strict

Now if you open another console and "telnet localhost 8080", you should be
tunneled through to the telnet service on your local machine (if it's running -
you could change it to port "22" and tunnel ssh instead if you so desired). When
you logout of the telnet session, the tunnel should cleanly shutdown and show
you some traffic stats in both consoles. Feel free to experiment. :-)

Notes:

 - the format for the "-listen" argument can skip the host part (eg. "-listen
   8080" is fine). If you do, the listening socket will listen on all interfaces
   so you can connect from other machines for example. Using the "localhost"
   form listens only on 127.0.0.1 so you can only connect locally (unless, of
   course, you've set up weird stuff with your networking in which case probably
   none of the above applies).

 - ./tunala -? gives you a list of other command-line options, but tunala.c is
   also a good place to look :-)


