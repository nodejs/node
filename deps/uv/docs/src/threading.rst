
.. _threading:

Threading and synchronization utilities
=======================================

libuv provides cross-platform implementations for multiple threading and
synchronization primitives. The API largely follows the pthreads API.


Data types
----------

.. c:type:: uv_thread_t

    Thread data type.

.. c:type:: void (*uv_thread_cb)(void* arg)

    Callback that is invoked to initialize thread execution. `arg` is the same
    value that was passed to :c:func:`uv_thread_create`.

.. c:type:: uv_key_t

    Thread-local key data type.

.. c:type:: uv_once_t

    Once-only initializer data type.

.. c:type:: uv_mutex_t

    Mutex data type.

.. c:type:: uv_rwlock_t

    Read-write lock data type.

.. c:type:: uv_sem_t

    Semaphore data type.

.. c:type:: uv_cond_t

    Condition data type.

.. c:type:: uv_barrier_t

    Barrier data type.


API
---

Threads
^^^^^^^

.. c:function:: int uv_thread_create(uv_thread_t* tid, uv_thread_cb entry, void* arg)

    .. versionchanged:: 1.4.1 returns a UV_E* error code on failure

.. c:function:: uv_thread_t uv_thread_self(void)
.. c:function:: int uv_thread_join(uv_thread_t *tid)
.. c:function:: int uv_thread_equal(const uv_thread_t* t1, const uv_thread_t* t2)

Thread-local storage
^^^^^^^^^^^^^^^^^^^^

.. note::
    The total thread-local storage size may be limited. That is, it may not be possible to
    create many TLS keys.

.. c:function:: int uv_key_create(uv_key_t* key)
.. c:function:: void uv_key_delete(uv_key_t* key)
.. c:function:: void* uv_key_get(uv_key_t* key)
.. c:function:: void uv_key_set(uv_key_t* key, void* value)

Once-only initialization
^^^^^^^^^^^^^^^^^^^^^^^^

Runs a function once and only once. Concurrent calls to :c:func:`uv_once` with the
same guard will block all callers except one (it's unspecified which one).
The guard should be initialized statically with the UV_ONCE_INIT macro.

.. c:function:: void uv_once(uv_once_t* guard, void (*callback)(void))

Mutex locks
^^^^^^^^^^^

Functions return 0 on success or an error code < 0 (unless the
return type is void, of course).

.. c:function:: int uv_mutex_init(uv_mutex_t* handle)
.. c:function:: int uv_mutex_init_recursive(uv_mutex_t* handle)
.. c:function:: void uv_mutex_destroy(uv_mutex_t* handle)
.. c:function:: void uv_mutex_lock(uv_mutex_t* handle)
.. c:function:: int uv_mutex_trylock(uv_mutex_t* handle)
.. c:function:: void uv_mutex_unlock(uv_mutex_t* handle)

Read-write locks
^^^^^^^^^^^^^^^^

Functions return 0 on success or an error code < 0 (unless the
return type is void, of course).

.. c:function:: int uv_rwlock_init(uv_rwlock_t* rwlock)
.. c:function:: void uv_rwlock_destroy(uv_rwlock_t* rwlock)
.. c:function:: void uv_rwlock_rdlock(uv_rwlock_t* rwlock)
.. c:function:: int uv_rwlock_tryrdlock(uv_rwlock_t* rwlock)
.. c:function:: void uv_rwlock_rdunlock(uv_rwlock_t* rwlock)
.. c:function:: void uv_rwlock_wrlock(uv_rwlock_t* rwlock)
.. c:function:: int uv_rwlock_trywrlock(uv_rwlock_t* rwlock)
.. c:function:: void uv_rwlock_wrunlock(uv_rwlock_t* rwlock)

Semaphores
^^^^^^^^^^

Functions return 0 on success or an error code < 0 (unless the
return type is void, of course).

.. c:function:: int uv_sem_init(uv_sem_t* sem, unsigned int value)
.. c:function:: void uv_sem_destroy(uv_sem_t* sem)
.. c:function:: void uv_sem_post(uv_sem_t* sem)
.. c:function:: void uv_sem_wait(uv_sem_t* sem)
.. c:function:: int uv_sem_trywait(uv_sem_t* sem)

Conditions
^^^^^^^^^^

Functions return 0 on success or an error code < 0 (unless the
return type is void, of course).

.. note::
    1. Callers should be prepared to deal with spurious wakeups on :c:func:`uv_cond_wait`
       and :c:func:`uv_cond_timedwait`.
    2. The timeout parameter for :c:func:`uv_cond_timedwait` is relative to the time
       at which function is called.
    3. On z/OS, the timeout parameter for :c:func:`uv_cond_timedwait` is converted to an
       absolute system time at which the wait expires. If the current system clock time
       passes the absolute time calculated before the condition is signaled, an ETIMEDOUT
       error results. After the wait begins, the wait time is not affected by changes
       to the system clock.

.. c:function:: int uv_cond_init(uv_cond_t* cond)
.. c:function:: void uv_cond_destroy(uv_cond_t* cond)
.. c:function:: void uv_cond_signal(uv_cond_t* cond)
.. c:function:: void uv_cond_broadcast(uv_cond_t* cond)
.. c:function:: void uv_cond_wait(uv_cond_t* cond, uv_mutex_t* mutex)
.. c:function:: int uv_cond_timedwait(uv_cond_t* cond, uv_mutex_t* mutex, uint64_t timeout)

Barriers
^^^^^^^^

Functions return 0 on success or an error code < 0 (unless the
return type is void, of course).

.. note::
    :c:func:`uv_barrier_wait` returns a value > 0 to an arbitrarily chosen "serializer" thread
    to facilitate cleanup, i.e.

    ::

        if (uv_barrier_wait(&barrier) > 0)
            uv_barrier_destroy(&barrier);

.. c:function:: int uv_barrier_init(uv_barrier_t* barrier, unsigned int count)
.. c:function:: void uv_barrier_destroy(uv_barrier_t* barrier)
.. c:function:: int uv_barrier_wait(uv_barrier_t* barrier)
