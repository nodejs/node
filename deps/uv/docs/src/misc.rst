
.. _misc:

Miscellaneous utilities
======================

This section contains miscellaneous functions that don't really belong in any
other section.


Data types
----------

.. c:type:: uv_buf_t

    Buffer data type.

.. c:type:: uv_file

    Cross platform representation of a file handle.

.. c:type:: uv_os_sock_t

    Cross platform representation of a socket handle.

.. c:type:: uv_os_fd_t

    Abstract representation of a file descriptor. On Unix systems this is a
    `typedef` of `int` and on Windows fa `HANDLE`.

.. c:type:: uv_rusage_t

    Data type for resource usage results.

    ::

        typedef struct {
            uv_timeval_t ru_utime; /* user CPU time used */
            uv_timeval_t ru_stime; /* system CPU time used */
            uint64_t ru_maxrss; /* maximum resident set size */
            uint64_t ru_ixrss; /* integral shared memory size */
            uint64_t ru_idrss; /* integral unshared data size */
            uint64_t ru_isrss; /* integral unshared stack size */
            uint64_t ru_minflt; /* page reclaims (soft page faults) */
            uint64_t ru_majflt; /* page faults (hard page faults) */
            uint64_t ru_nswap; /* swaps */
            uint64_t ru_inblock; /* block input operations */
            uint64_t ru_oublock; /* block output operations */
            uint64_t ru_msgsnd; /* IPC messages sent */
            uint64_t ru_msgrcv; /* IPC messages received */
            uint64_t ru_nsignals; /* signals received */
            uint64_t ru_nvcsw; /* voluntary context switches */
            uint64_t ru_nivcsw; /* involuntary context switches */
        } uv_rusage_t;

.. c:type:: uv_cpu_info_t

    Data type for CPU information.

    ::

        typedef struct uv_cpu_info_s {
            char* model;
            int speed;
            struct uv_cpu_times_s {
                uint64_t user;
                uint64_t nice;
                uint64_t sys;
                uint64_t idle;
                uint64_t irq;
            } cpu_times;
        } uv_cpu_info_t;

.. c:type:: uv_interface_address_t

    Data type for interface addresses.

    ::

        typedef struct uv_interface_address_s {
            char* name;
            char phys_addr[6];
            int is_internal;
            union {
                struct sockaddr_in address4;
                struct sockaddr_in6 address6;
            } address;
            union {
                struct sockaddr_in netmask4;
                struct sockaddr_in6 netmask6;
            } netmask;
        } uv_interface_address_t;


API
---

.. c:function:: uv_handle_type uv_guess_handle(uv_file file)

    Used to detect what type of stream should be used with a given file
    descriptor. Usually this will be used during initialization to guess the
    type of the stdio streams.

    For ``isatty()`` functionality use this function and test for ``UV_TTY``.

.. c:function:: unsigned int uv_version(void)

    Returns the libuv version packed into a single integer. 8 bits are used for
    each component, with the patch number stored in the 8 least significant
    bits. E.g. for libuv 1.2.3 this would return 0x010203.

.. c:function:: const char* uv_version_string(void)

    Returns the libuv version number as a string. For non-release versions
    "-pre" is appended, so the version number could be "1.2.3-pre".

.. c:function:: uv_buf_t uv_buf_init(char* base, unsigned int len)

    Constructor for :c:type:`uv_buf_t`.

    Due to platform differences the user cannot rely on the ordering of the
    `base` and `len` members of the uv_buf_t struct. The user is responsible for
    freeing `base` after the uv_buf_t is done. Return struct passed by value.

.. c:function:: char** uv_setup_args(int argc, char** argv)

    Store the program arguments. Required for getting / setting the process title.

.. c:function:: int uv_get_process_title(char* buffer, size_t size)

    Gets the title of the current process.

.. c:function:: int uv_set_process_title(const char* title)

    Sets the current process title.

.. c:function:: int uv_resident_set_memory(size_t* rss)

    Gets the resident set size (RSS) for the current process.

.. c:function:: int uv_uptime(double* uptime)

    Gets the current system uptime.

.. c:function:: int uv_getrusage(uv_rusage_t* rusage)

    Gets the resource usage measures for the current process.

    .. note::
        On Windows not all fields are set, the unsupported fields are filled with zeroes.

.. c:function:: int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count)

    Gets information about the CPUs on the system. The `cpu_infos` array will
    have `count` elements and needs to be freed with :c:func:`uv_free_cpu_info`.

.. c:function:: void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count)

    Frees the `cpu_infos` array previously allocated with :c:func:`uv_cpu_info`.

.. c:function:: int uv_interface_addresses(uv_interface_address_t** addresses, int* count)

    Gets address information about the network interfaces on the system. An
    array of `count` elements is allocated and returned in `addresses`. It must
    be freed by the user, calling :c:func:`uv_free_interface_addresses`.

.. c:function:: void uv_free_interface_addresses(uv_interface_address_t* addresses, int count)

    Free an array of :c:type:`uv_interface_address_t` which was returned by
    :c:func:`uv_interface_addresses`.

.. c:function:: void uv_loadavg(double avg[3])

    Gets the load average. See: `<http://en.wikipedia.org/wiki/Load_(computing)>`_

    .. note::
        Returns [0,0,0] on Windows (i.e., it's not implemented).

.. c:function:: int uv_ip4_addr(const char* ip, int port, struct sockaddr_in* addr)

    Convert a string containing an IPv4 addresses to a binary structure.

.. c:function:: int uv_ip6_addr(const char* ip, int port, struct sockaddr_in6* addr)

    Convert a string containing an IPv6 addresses to a binary structure.

.. c:function:: int uv_ip4_name(const struct sockaddr_in* src, char* dst, size_t size)

    Convert a binary structure containing an IPv4 address to a string.

.. c:function:: int uv_ip6_name(const struct sockaddr_in6* src, char* dst, size_t size)

    Convert a binary structure containing an IPv6 address to a string.

.. c:function:: int uv_inet_ntop(int af, const void* src, char* dst, size_t size)
.. c:function:: int uv_inet_pton(int af, const char* src, void* dst)

    Cross-platform IPv6-capable implementation of the 'standard' ``inet_ntop()``
    and ``inet_pton()`` functions. On success they return 0. In case of error
    the target `dst` pointer is unmodified.

.. c:function:: int uv_exepath(char* buffer, size_t* size)

    Gets the executable path.

.. c:function:: int uv_cwd(char* buffer, size_t* size)

    Gets the current working directory.

.. c:function:: int uv_chdir(const char* dir)

    Changes the current working directory.

.. uint64_t uv_get_free_memory(void)
.. c:function:: uint64_t uv_get_total_memory(void)

    Gets memory information (in bytes).

.. c:function:: uint64_t uv_hrtime(void)

    Returns the current high-resolution real time. This is expressed in
    nanoseconds. It is relative to an arbitrary time in the past. It is not
    related to the time of day and therefore not subject to clock drift. The
    primary use is for measuring performance between intervals.

    .. note::
        Not every platform can support nanosecond resolution; however, this value will always
        be in nanoseconds.
