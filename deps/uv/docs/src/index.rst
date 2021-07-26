
Welcome to the libuv documentation
==================================

Overview
--------

libuv is a multi-platform support library with a focus on asynchronous I/O. It
was primarily developed for use by `Node.js`_, but it's also used by `Luvit`_,
`Julia`_, `pyuv`_, and `others`_.

.. note::
    In case you find errors in this documentation you can help by sending
    `pull requests <https://github.com/libuv/libuv>`_!

.. _Node.js: https://nodejs.org
.. _Luvit: https://luvit.io
.. _Julia: https://julialang.org
.. _pyuv: https://github.com/saghul/pyuv
.. _others: https://github.com/libuv/libuv/blob/v1.x/LINKS.md


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


Documentation
-------------

.. toctree::
   :maxdepth: 1

   design
   api
   guide
   upgrading


Downloads
---------

libuv can be downloaded from `here <https://dist.libuv.org/dist/>`_.


Installation
------------

Installation instructions can be found in `the README <https://github.com/libuv/libuv/blob/master/README.md>`_.

