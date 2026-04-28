
.. _dll:

Shared library handling
=======================

libuv provides cross platform utilities for loading shared libraries and
retrieving symbols from them, using the following API.


Data types
----------

.. c:type:: uv_lib_t

    Shared library data type.


Public members
^^^^^^^^^^^^^^

N/A


API
---

.. c:function:: int uv_dlopen(const char* filename, uv_lib_t* lib)

    Opens a shared library. The filename is in utf-8. Returns 0 on success and
    -1 on error. Call :c:func:`uv_dlerror` to get the error message.

.. c:function:: void uv_dlclose(uv_lib_t* lib)

    Close the shared library.

.. c:function:: int uv_dlsym(uv_lib_t* lib, const char* name, void** ptr)

    Retrieves a data pointer from a dynamic library. It is legal for a symbol
    to map to NULL. Returns 0 on success and -1 if the symbol was not found.

.. c:function:: const char* uv_dlerror(const uv_lib_t* lib)

    Returns the last uv_dlopen() or uv_dlsym() error message.
