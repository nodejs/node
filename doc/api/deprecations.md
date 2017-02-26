# Deprecated APIs

Node.js may deprecate APIs when either: (a) use of the API is considered to be
unsafe, (b) an improved alternative API has been made available, or (c)
breaking changes to the API are expected in a future major release.

Node.js utilizes three kinds of Deprecations:

* Documentation-only
* Runtime
* End-of-Life

A Documentation-only deprecation is one that is expressed only within the
Node.js API docs. These generate no side-effects while running Node.js.

A Runtime deprecation will, by default, generate a process warning that will
be printed to `stderr` the first time the deprecated API is used. When the
`--throw-deprecation` command-line flag is used, a Runtime deprecation will
cause an error to be thrown.

An End-of-Life deprecation is used to identify code that either has been
removed or will soon be removed from Node.js.

## Un-deprecation

From time-to-time the deprecation of an API may be reversed. Such action may
happen in either a semver-minor or semver-major release. In such situations,
this document will be updated with information relevant to the decision.
*However, the deprecation identifier will not be modified*.

## List of Deprecated APIs

<a id="DEP0062"></a>
### DEP0062: node --debug

Type: Runtime

`--debug` activates the legacy V8 debugger interface, which has been removed as
of V8 5.8. It is replaced by Inspector which is activated with `--inspect`
instead.

<a id="DEP0063"></a>
### DEP0063: ServerResponse.prototype.writeHeader()

Type: Documentation-only

The `http` module `ServerResponse.prototype.writeHeader()` API has been
deprecated. Please use `ServerResponse.prototype.writeHead()` instead.

*Note*: The `ServerResponse.prototype.writeHeader()` method was never documented
as an officially supported API.

[alloc]: buffer.html#buffer_class_method_buffer_alloc_size_fill_encoding
[alloc_unsafe_size]: buffer.html#buffer_class_method_buffer_allocunsafe_size
[`Buffer.allocUnsafeSlow(size)`]: buffer.html#buffer_class_method_buffer_allocunsafeslow_size
[`Buffer.isBuffer()`]: buffer.html#buffer_class_method_buffer_isbuffer_obj
[`Buffer.from(array)`]: buffer.html#buffer_class_method_buffer_from_array
[from_arraybuffer]: buffer.html#buffer_class_method_buffer_from_arraybuffer_byteoffset_length
[`Buffer.from(buffer)`]: buffer.html#buffer_class_method_buffer_from_buffer
[from_string_encoding]: buffer.html#buffer_class_method_buffer_from_string_encoding
[`SlowBuffer`]: buffer.html#buffer_class_slowbuffer
[`child_process`]: child_process.html
[`console.error()`]: console.html#console_console_error_data_args
[`console.log()`]: console.html#console_console_log_data_args
[`crypto.createCredentials()`]: crypto.html#crypto_crypto_createcredentials_details
[`crypto.pbkdf2()`]: crypto.html#crypto_crypto_pbkdf2_password_salt_iterations_keylen_digest_callback
[`domain`]: domain.html
[`Domain.dispose()`]: domain.html#domain_domain_dispose
[`ecdh.setPublicKey()`]: crypto.html#crypto_ecdh_setpublickey_public_key_encoding
[`emitter.listenerCount(eventName)`]: events.html#events_emitter_listenercount_eventname
[`EventEmitter.listenerCount(emitter, eventName)`]: events.html#events_eventemitter_listenercount_emitter_eventname
[`fs.exists(path, callback)`]: fs.html#fs_fs_exists_path_callback
[`fs.stat()`]: fs.html#fs_fs_stat_path_callback
[`fs.access()`]: fs.html#fs_fs_access_path_mode_callback
[`fs.lchmod(path, mode, callback)`]: fs.html#fs_fs_lchmod_path_mode_callback
[`fs.lchmodSync(path, mode)`]: fs.html#fs_fs_lchmodsync_path_mode
[`fs.lchown(path, uid, gid, callback)`]: fs.html#fs_fs_lchown_path_uid_gid_callback
[`fs.lchownSync(path, uid, gid)`]: fs.html#fs_fs_lchownsync_path_uid_gid
[`fs.read()`]: fs.html#fs_fs_read_fd_buffer_offset_length_position_callback
[`fs.readSync()`]: fs.html#fs_fs_readsync_fd_buffer_offset_length_position
[`Server.connections`]: net.html#net_server_connections
[`Server.getConnections()`]: net.html#net_server_getconnections_callback
[`Server.listen({fd: <number>})`]: net.html#net_server_listen_handle_backlog_callback
[`os.networkInterfaces`]: os.html#os_os_networkinterfaces
[`os.tmpdir()`]: os.html#os_os_tmpdir
[`punycode`]: punycode.html
[`require.extensions`]: globals.html#globals_require_extensions
[`tls.TLSSocket`]: tls.html#tls_class_tls_tlssocket
[`tls.CryptoStream`]: tls.html#tls_class_cryptostream
[`tls.SecurePair`]: tls.html#tls_class_securepair
[`tls.SecureContext`]: tls.html#tls_tls_createsecurecontext_options
[`tls.createSecureContext()`]: tls.html#tls_tls_createsecurecontext_options
[`util`]: util.html
[`util.debug()`]: util.html#util_util_debug_string
[`util.error()`]: util.html#util_util_error_strings
[`util.puts()`]: util.html#util_util_puts_strings
[`util.print()`]: util.html#util_util_print_strings
[`util.isArray()`]: util.html#util_util_isarray_object
[`util.isBoolean()`]: util.html#util_util_isboolean_object
[`util.isBuffer()`]: util.html#util_util_isbuffer_object
[`util.isDate()`]: util.html#util_util_isdate_object
[`util.isError()`]: util.html#util_util_iserror_object
[`util.isFunction()`]: util.html#util_util_isfunction_object
[`util.isNull()`]: util.html#util_util_isnull_object
[`util.isNullOrUndefined()`]: util.html#util_util_isnullorundefined_object
[`util.isNumber()`]: util.html#util_util_isnumber_object
[`util.isObject()`]: util.html#util_util_isobject_object
[`util.isPrimitive()`]: util.html#util_util_isprimitive_object
[`util.isRegExp()`]: util.html#util_util_isregexp_object
[`util.isString()`]: util.html#util_util_isstring_object
[`util.isSymbol()`]: util.html#util_util_issymbol_object
[`util.isUndefined()`]: util.html#util_util_isundefined_object
[`util.log()`]: util.html#util_util_log_string
[`util._extend()`]: util.html#util_util_extend_target_source
[`worker.suicide`]: cluster.html#cluster_worker_suicide
[`worker.exitedAfterDisconnect`]: cluster.html#cluster_worker_exitedafterdisconnect
