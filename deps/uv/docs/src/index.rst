
Welcome to the libuv API documentation
======================================

Overview
--------

libuv is a multi-platform support library with a focus on asynchronous I/O. It
was primarily developed for use by `Node.js`_, but it's also used by `Luvit`_,
`Julia`_, `pyuv`_, and `others`_.

.. note::
    In case you find errors in this documentation you can help by sending
    `pull requests <https://github.com/libuv/libuv>`_!

.. _Node.js: http://nodejs.org
.. _Luvit: http://luvit.io
.. _Julia: http://julialang.org
.. _pyuv: https://github.com/saghul/pyuv
.. _others: https://github.com/libuv/libuv/wiki/Projects-that-use-libuv


Features
--------

* Full-featured event loop backed by epoll, kqueue, IOCP, event ports.
* Asynchronous TCP and UDP sockets
* Asynchronous DNS resolution
* Asynchronous file and file system operations
* File system events
* ANSI escape code controlled TTY
* IPC with socket sharing, using Unix domain sockets or named pipes (Windows)
* Child processes
* Thread pool
* Signal handling
* High resolution clock
* Threading and synchronization primitives


Downloads
---------

libuv can be downloaded from `here <http://dist.libuv.org/dist/>`_.


Installation
------------

Installation instructions can be found on `the README <https://github.com/libuv/libuv/blob/master/README.md>`_.


Upgrading
---------

Migration guides for different libuv versions, starting with 1.0.

.. toctree::
   :maxdepth: 1

   migration_010_100


Documentation
-------------

.. toctree::
   :maxdepth: 1

   design
   errors
   version
   loop
   handle
   request
   timer
   prepare
   check
   idle
   async
   poll
   signal
   process
   stream
   tcp
   pipe
   tty
   udp
   fs_event
   fs_poll
   fs
   threadpool
   dns
   dll
   threading
   misc
