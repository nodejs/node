"""The optional bytecode cache system. This is useful if you have very
complex template situations and the compilation of all those templates
slows down your application too much.

Situations where this is useful are often forking web applications that
are initialized on the first request.
"""
import errno
import fnmatch
import marshal
import os
import pickle
import stat
import sys
import tempfile
import typing as t
from hashlib import sha1
from io import BytesIO
from types import CodeType

if t.TYPE_CHECKING:
    import typing_extensions as te
    from .environment import Environment

    class _MemcachedClient(te.Protocol):
        def get(self, key: str) -> bytes:
            ...

        def set(self, key: str, value: bytes, timeout: t.Optional[int] = None) -> None:
            ...


bc_version = 5
# Magic bytes to identify Jinja bytecode cache files. Contains the
# Python major and minor version to avoid loading incompatible bytecode
# if a project upgrades its Python version.
bc_magic = (
    b"j2"
    + pickle.dumps(bc_version, 2)
    + pickle.dumps((sys.version_info[0] << 24) | sys.version_info[1], 2)
)


class Bucket:
    """Buckets are used to store the bytecode for one template.  It's created
    and initialized by the bytecode cache and passed to the loading functions.

    The buckets get an internal checksum from the cache assigned and use this
    to automatically reject outdated cache material.  Individual bytecode
    cache subclasses don't have to care about cache invalidation.
    """

    def __init__(self, environment: "Environment", key: str, checksum: str) -> None:
        self.environment = environment
        self.key = key
        self.checksum = checksum
        self.reset()

    def reset(self) -> None:
        """Resets the bucket (unloads the bytecode)."""
        self.code: t.Optional[CodeType] = None

    def load_bytecode(self, f: t.BinaryIO) -> None:
        """Loads bytecode from a file or file like object."""
        # make sure the magic header is correct
        magic = f.read(len(bc_magic))
        if magic != bc_magic:
            self.reset()
            return
        # the source code of the file changed, we need to reload
        checksum = pickle.load(f)
        if self.checksum != checksum:
            self.reset()
            return
        # if marshal_load fails then we need to reload
        try:
            self.code = marshal.load(f)
        except (EOFError, ValueError, TypeError):
            self.reset()
            return

    def write_bytecode(self, f: t.IO[bytes]) -> None:
        """Dump the bytecode into the file or file like object passed."""
        if self.code is None:
            raise TypeError("can't write empty bucket")
        f.write(bc_magic)
        pickle.dump(self.checksum, f, 2)
        marshal.dump(self.code, f)

    def bytecode_from_string(self, string: bytes) -> None:
        """Load bytecode from bytes."""
        self.load_bytecode(BytesIO(string))

    def bytecode_to_string(self) -> bytes:
        """Return the bytecode as bytes."""
        out = BytesIO()
        self.write_bytecode(out)
        return out.getvalue()


class BytecodeCache:
    """To implement your own bytecode cache you have to subclass this class
    and override :meth:`load_bytecode` and :meth:`dump_bytecode`.  Both of
    these methods are passed a :class:`~jinja2.bccache.Bucket`.

    A very basic bytecode cache that saves the bytecode on the file system::

        from os import path

        class MyCache(BytecodeCache):

            def __init__(self, directory):
                self.directory = directory

            def load_bytecode(self, bucket):
                filename = path.join(self.directory, bucket.key)
                if path.exists(filename):
                    with open(filename, 'rb') as f:
                        bucket.load_bytecode(f)

            def dump_bytecode(self, bucket):
                filename = path.join(self.directory, bucket.key)
                with open(filename, 'wb') as f:
                    bucket.write_bytecode(f)

    A more advanced version of a filesystem based bytecode cache is part of
    Jinja.
    """

    def load_bytecode(self, bucket: Bucket) -> None:
        """Subclasses have to override this method to load bytecode into a
        bucket.  If they are not able to find code in the cache for the
        bucket, it must not do anything.
        """
        raise NotImplementedError()

    def dump_bytecode(self, bucket: Bucket) -> None:
        """Subclasses have to override this method to write the bytecode
        from a bucket back to the cache.  If it unable to do so it must not
        fail silently but raise an exception.
        """
        raise NotImplementedError()

    def clear(self) -> None:
        """Clears the cache.  This method is not used by Jinja but should be
        implemented to allow applications to clear the bytecode cache used
        by a particular environment.
        """

    def get_cache_key(
        self, name: str, filename: t.Optional[t.Union[str]] = None
    ) -> str:
        """Returns the unique hash key for this template name."""
        hash = sha1(name.encode("utf-8"))

        if filename is not None:
            hash.update(f"|{filename}".encode())

        return hash.hexdigest()

    def get_source_checksum(self, source: str) -> str:
        """Returns a checksum for the source."""
        return sha1(source.encode("utf-8")).hexdigest()

    def get_bucket(
        self,
        environment: "Environment",
        name: str,
        filename: t.Optional[str],
        source: str,
    ) -> Bucket:
        """Return a cache bucket for the given template.  All arguments are
        mandatory but filename may be `None`.
        """
        key = self.get_cache_key(name, filename)
        checksum = self.get_source_checksum(source)
        bucket = Bucket(environment, key, checksum)
        self.load_bytecode(bucket)
        return bucket

    def set_bucket(self, bucket: Bucket) -> None:
        """Put the bucket into the cache."""
        self.dump_bytecode(bucket)


class FileSystemBytecodeCache(BytecodeCache):
    """A bytecode cache that stores bytecode on the filesystem.  It accepts
    two arguments: The directory where the cache items are stored and a
    pattern string that is used to build the filename.

    If no directory is specified a default cache directory is selected.  On
    Windows the user's temp directory is used, on UNIX systems a directory
    is created for the user in the system temp directory.

    The pattern can be used to have multiple separate caches operate on the
    same directory.  The default pattern is ``'__jinja2_%s.cache'``.  ``%s``
    is replaced with the cache key.

    >>> bcc = FileSystemBytecodeCache('/tmp/jinja_cache', '%s.cache')

    This bytecode cache supports clearing of the cache using the clear method.
    """

    def __init__(
        self, directory: t.Optional[str] = None, pattern: str = "__jinja2_%s.cache"
    ) -> None:
        if directory is None:
            directory = self._get_default_cache_dir()
        self.directory = directory
        self.pattern = pattern

    def _get_default_cache_dir(self) -> str:
        def _unsafe_dir() -> "te.NoReturn":
            raise RuntimeError(
                "Cannot determine safe temp directory.  You "
                "need to explicitly provide one."
            )

        tmpdir = tempfile.gettempdir()

        # On windows the temporary directory is used specific unless
        # explicitly forced otherwise.  We can just use that.
        if os.name == "nt":
            return tmpdir
        if not hasattr(os, "getuid"):
            _unsafe_dir()

        dirname = f"_jinja2-cache-{os.getuid()}"
        actual_dir = os.path.join(tmpdir, dirname)

        try:
            os.mkdir(actual_dir, stat.S_IRWXU)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise
        try:
            os.chmod(actual_dir, stat.S_IRWXU)
            actual_dir_stat = os.lstat(actual_dir)
            if (
                actual_dir_stat.st_uid != os.getuid()
                or not stat.S_ISDIR(actual_dir_stat.st_mode)
                or stat.S_IMODE(actual_dir_stat.st_mode) != stat.S_IRWXU
            ):
                _unsafe_dir()
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

        actual_dir_stat = os.lstat(actual_dir)
        if (
            actual_dir_stat.st_uid != os.getuid()
            or not stat.S_ISDIR(actual_dir_stat.st_mode)
            or stat.S_IMODE(actual_dir_stat.st_mode) != stat.S_IRWXU
        ):
            _unsafe_dir()

        return actual_dir

    def _get_cache_filename(self, bucket: Bucket) -> str:
        return os.path.join(self.directory, self.pattern % (bucket.key,))

    def load_bytecode(self, bucket: Bucket) -> None:
        filename = self._get_cache_filename(bucket)

        # Don't test for existence before opening the file, since the
        # file could disappear after the test before the open.
        try:
            f = open(filename, "rb")
        except (FileNotFoundError, IsADirectoryError, PermissionError):
            # PermissionError can occur on Windows when an operation is
            # in progress, such as calling clear().
            return

        with f:
            bucket.load_bytecode(f)

    def dump_bytecode(self, bucket: Bucket) -> None:
        # Write to a temporary file, then rename to the real name after
        # writing. This avoids another process reading the file before
        # it is fully written.
        name = self._get_cache_filename(bucket)
        f = tempfile.NamedTemporaryFile(
            mode="wb",
            dir=os.path.dirname(name),
            prefix=os.path.basename(name),
            suffix=".tmp",
            delete=False,
        )

        def remove_silent() -> None:
            try:
                os.remove(f.name)
            except OSError:
                # Another process may have called clear(). On Windows,
                # another program may be holding the file open.
                pass

        try:
            with f:
                bucket.write_bytecode(f)
        except BaseException:
            remove_silent()
            raise

        try:
            os.replace(f.name, name)
        except OSError:
            # Another process may have called clear(). On Windows,
            # another program may be holding the file open.
            remove_silent()
        except BaseException:
            remove_silent()
            raise

    def clear(self) -> None:
        # imported lazily here because google app-engine doesn't support
        # write access on the file system and the function does not exist
        # normally.
        from os import remove

        files = fnmatch.filter(os.listdir(self.directory), self.pattern % ("*",))
        for filename in files:
            try:
                remove(os.path.join(self.directory, filename))
            except OSError:
                pass


class MemcachedBytecodeCache(BytecodeCache):
    """This class implements a bytecode cache that uses a memcache cache for
    storing the information.  It does not enforce a specific memcache library
    (tummy's memcache or cmemcache) but will accept any class that provides
    the minimal interface required.

    Libraries compatible with this class:

    -   `cachelib <https://github.com/pallets/cachelib>`_
    -   `python-memcached <https://pypi.org/project/python-memcached/>`_

    (Unfortunately the django cache interface is not compatible because it
    does not support storing binary data, only text. You can however pass
    the underlying cache client to the bytecode cache which is available
    as `django.core.cache.cache._client`.)

    The minimal interface for the client passed to the constructor is this:

    .. class:: MinimalClientInterface

        .. method:: set(key, value[, timeout])

            Stores the bytecode in the cache.  `value` is a string and
            `timeout` the timeout of the key.  If timeout is not provided
            a default timeout or no timeout should be assumed, if it's
            provided it's an integer with the number of seconds the cache
            item should exist.

        .. method:: get(key)

            Returns the value for the cache key.  If the item does not
            exist in the cache the return value must be `None`.

    The other arguments to the constructor are the prefix for all keys that
    is added before the actual cache key and the timeout for the bytecode in
    the cache system.  We recommend a high (or no) timeout.

    This bytecode cache does not support clearing of used items in the cache.
    The clear method is a no-operation function.

    .. versionadded:: 2.7
       Added support for ignoring memcache errors through the
       `ignore_memcache_errors` parameter.
    """

    def __init__(
        self,
        client: "_MemcachedClient",
        prefix: str = "jinja2/bytecode/",
        timeout: t.Optional[int] = None,
        ignore_memcache_errors: bool = True,
    ):
        self.client = client
        self.prefix = prefix
        self.timeout = timeout
        self.ignore_memcache_errors = ignore_memcache_errors

    def load_bytecode(self, bucket: Bucket) -> None:
        try:
            code = self.client.get(self.prefix + bucket.key)
        except Exception:
            if not self.ignore_memcache_errors:
                raise
        else:
            bucket.bytecode_from_string(code)

    def dump_bytecode(self, bucket: Bucket) -> None:
        key = self.prefix + bucket.key
        value = bucket.bytecode_to_string()

        try:
            if self.timeout is not None:
                self.client.set(key, value, self.timeout)
            else:
                self.client.set(key, value)
        except Exception:
            if not self.ignore_memcache_errors:
                raise
