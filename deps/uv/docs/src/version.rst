
.. _version:

Version-checking macros and functions
=====================================

Starting with version 1.0.0 libuv follows the `semantic versioning`_
scheme. This means that new APIs can be introduced throughout the lifetime of
a major release. In this section you'll find all macros and functions that
will allow you to write or compile code conditionally, in order to work with
multiple libuv versions.

.. _semantic versioning: https://semver.org


Macros
------

.. c:macro:: UV_VERSION_MAJOR

    libuv version's major number.

.. c:macro:: UV_VERSION_MINOR

    libuv version's minor number.

.. c:macro:: UV_VERSION_PATCH

    libuv version's patch number.

.. c:macro:: UV_VERSION_IS_RELEASE

    Set to 1 to indicate a release version of libuv, 0 for a development
    snapshot.

.. c:macro:: UV_VERSION_SUFFIX

    libuv version suffix. Certain development releases such as Release Candidates
    might have a suffix such as "rc".

.. c:macro:: UV_VERSION_HEX

    Returns the libuv version packed into a single integer. 8 bits are used for
    each component, with the patch number stored in the 8 least significant
    bits. E.g. for libuv 1.2.3 this would be 0x010203.

    .. versionadded:: 1.7.0


Functions
---------

.. c:function:: unsigned int uv_version(void)

    Returns :c:macro:`UV_VERSION_HEX`.

.. c:function:: const char* uv_version_string(void)

    Returns the libuv version number as a string. For non-release versions the
    version suffix is included.
