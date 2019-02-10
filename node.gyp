{
  'variables': {
    'v8_use_snapshot%': 'false',
    'v8_trace_maps%': 0,
    'node_use_dtrace%': 'false',
    'node_use_etw%': 'false',
    'node_no_browser_globals%': 'false',
    'node_code_cache_path%': '',
    'node_use_v8_platform%': 'true',
    'node_use_bundled_v8%': 'true',
    'node_shared%': 'false',
    'force_dynamic_crt%': 0,
    'node_module_version%': '',
    'node_shared_brotli%': 'false',
    'node_shared_zlib%': 'false',
    'node_shared_http_parser%': 'false',
    'node_shared_cares%': 'false',
    'node_shared_libuv%': 'false',
    'node_shared_nghttp2%': 'false',
    'node_use_openssl%': 'true',
    'node_shared_openssl%': 'false',
    'node_v8_options%': '',
    'node_enable_v8_vtunejit%': 'false',
    'node_core_target_name%': 'node',
    'node_lib_target_name%': 'node_lib',
    'node_intermediate_lib_type%': 'static_library',
    'library_files': [
      'lib/internal/bootstrap/context.js',
      'lib/internal/bootstrap/primordials.js',
      'lib/internal/bootstrap/cache.js',
      'lib/internal/bootstrap/loaders.js',
      'lib/internal/bootstrap/node.js',
      'lib/internal/bootstrap/pre_execution.js',
      'lib/async_hooks.js',
      'lib/assert.js',
      'lib/buffer.js',
      'lib/child_process.js',
      'lib/console.js',
      'lib/constants.js',
      'lib/crypto.js',
      'lib/cluster.js',
      'lib/dgram.js',
      'lib/dns.js',
      'lib/domain.js',
      'lib/events.js',
      'lib/fs.js',
      'lib/http.js',
      'lib/http2.js',
      'lib/_http_agent.js',
      'lib/_http_client.js',
      'lib/_http_common.js',
      'lib/_http_incoming.js',
      'lib/_http_outgoing.js',
      'lib/_http_server.js',
      'lib/https.js',
      'lib/inspector.js',
      'lib/module.js',
      'lib/net.js',
      'lib/os.js',
      'lib/path.js',
      'lib/perf_hooks.js',
      'lib/process.js',
      'lib/punycode.js',
      'lib/querystring.js',
      'lib/readline.js',
      'lib/repl.js',
      'lib/stream.js',
      'lib/_stream_readable.js',
      'lib/_stream_writable.js',
      'lib/_stream_duplex.js',
      'lib/_stream_transform.js',
      'lib/_stream_passthrough.js',
      'lib/_stream_wrap.js',
      'lib/string_decoder.js',
      'lib/sys.js',
      'lib/timers.js',
      'lib/tls.js',
      'lib/_tls_common.js',
      'lib/_tls_wrap.js',
      'lib/trace_events.js',
      'lib/tty.js',
      'lib/url.js',
      'lib/util.js',
      'lib/v8.js',
      'lib/vm.js',
      'lib/worker_threads.js',
      'lib/zlib.js',
      'lib/internal/assert.js',
      'lib/internal/assert/assertion_error.js',
      'lib/internal/async_hooks.js',
      'lib/internal/buffer.js',
      'lib/internal/cli_table.js',
      'lib/internal/child_process.js',
      'lib/internal/cluster/child.js',
      'lib/internal/cluster/master.js',
      'lib/internal/cluster/round_robin_handle.js',
      'lib/internal/cluster/shared_handle.js',
      'lib/internal/cluster/utils.js',
      'lib/internal/cluster/worker.js',
      'lib/internal/console/constructor.js',
      'lib/internal/console/global.js',
      'lib/internal/coverage-gen/with_profiler.js',
      'lib/internal/crypto/certificate.js',
      'lib/internal/crypto/cipher.js',
      'lib/internal/crypto/diffiehellman.js',
      'lib/internal/crypto/hash.js',
      'lib/internal/crypto/keygen.js',
      'lib/internal/crypto/keys.js',
      'lib/internal/crypto/pbkdf2.js',
      'lib/internal/crypto/random.js',
      'lib/internal/crypto/scrypt.js',
      'lib/internal/crypto/sig.js',
      'lib/internal/crypto/util.js',
      'lib/internal/constants.js',
      'lib/internal/dgram.js',
      'lib/internal/dns/promises.js',
      'lib/internal/dns/utils.js',
      'lib/internal/domexception.js',
      'lib/internal/encoding.js',
      'lib/internal/errors.js',
      'lib/internal/error-serdes.js',
      'lib/internal/fixed_queue.js',
      'lib/internal/freelist.js',
      'lib/internal/fs/promises.js',
      'lib/internal/fs/read_file_context.js',
      'lib/internal/fs/streams.js',
      'lib/internal/fs/sync_write_stream.js',
      'lib/internal/fs/utils.js',
      'lib/internal/fs/watchers.js',
      'lib/internal/http.js',
      'lib/internal/idna.js',
      'lib/internal/inspector_async_hook.js',
      'lib/internal/js_stream_socket.js',
      'lib/internal/linkedlist.js',
      'lib/internal/main/check_syntax.js',
      'lib/internal/main/eval_string.js',
      'lib/internal/main/eval_stdin.js',
      'lib/internal/main/inspect.js',
      'lib/internal/main/print_bash_completion.js',
      'lib/internal/main/print_help.js',
      'lib/internal/main/prof_process.js',
      'lib/internal/main/repl.js',
      'lib/internal/main/run_main_module.js',
      'lib/internal/main/run_third_party_main.js',
      'lib/internal/main/worker_thread.js',
      'lib/internal/modules/cjs/helpers.js',
      'lib/internal/modules/cjs/loader.js',
      'lib/internal/modules/esm/loader.js',
      'lib/internal/modules/esm/create_dynamic_module.js',
      'lib/internal/modules/esm/default_resolve.js',
      'lib/internal/modules/esm/module_job.js',
      'lib/internal/modules/esm/module_map.js',
      'lib/internal/modules/esm/translators.js',
      'lib/internal/net.js',
      'lib/internal/options.js',
      'lib/internal/policy/manifest.js',
      'lib/internal/policy/sri.js',
      'lib/internal/priority_queue.js',
      'lib/internal/process/esm_loader.js',
      'lib/internal/process/execution.js',
      'lib/internal/process/main_thread_only.js',
      'lib/internal/process/next_tick.js',
      'lib/internal/process/per_thread.js',
      'lib/internal/process/policy.js',
      'lib/internal/process/promises.js',
      'lib/internal/process/stdio.js',
      'lib/internal/process/warning.js',
      'lib/internal/process/worker_thread_only.js',
      'lib/internal/process/report.js',
      'lib/internal/querystring.js',
      'lib/internal/queue_microtask.js',
      'lib/internal/readline.js',
      'lib/internal/repl.js',
      'lib/internal/repl/await.js',
      'lib/internal/repl/history.js',
      'lib/internal/repl/recoverable.js',
      'lib/internal/socket_list.js',
      'lib/internal/test/binding.js',
      'lib/internal/test/heap.js',
      'lib/internal/timers.js',
      'lib/internal/tls.js',
      'lib/internal/trace_events_async_hooks.js',
      'lib/internal/tty.js',
      'lib/internal/url.js',
      'lib/internal/util.js',
      'lib/internal/util/comparisons.js',
      'lib/internal/util/inspect.js',
      'lib/internal/util/inspector.js',
      'lib/internal/util/types.js',
      'lib/internal/http2/core.js',
      'lib/internal/http2/compat.js',
      'lib/internal/http2/util.js',
      'lib/internal/v8_prof_polyfill.js',
      'lib/internal/v8_prof_processor.js',
      'lib/internal/validators.js',
      'lib/internal/stream_base_commons.js',
      'lib/internal/vm/source_text_module.js',
      'lib/internal/worker.js',
      'lib/internal/worker/io.js',
      'lib/internal/streams/lazy_transform.js',
      'lib/internal/streams/async_iterator.js',
      'lib/internal/streams/buffer_list.js',
      'lib/internal/streams/duplexpair.js',
      'lib/internal/streams/legacy.js',
      'lib/internal/streams/destroy.js',
      'lib/internal/streams/state.js',
      'lib/internal/streams/pipeline.js',
      'lib/internal/streams/end-of-stream.js',
      'deps/v8/tools/splaytree.js',
      'deps/v8/tools/codemap.js',
      'deps/v8/tools/consarray.js',
      'deps/v8/tools/csvparser.js',
      'deps/v8/tools/profile.js',
      'deps/v8/tools/profile_view.js',
      'deps/v8/tools/logreader.js',
      'deps/v8/tools/arguments.js',
      'deps/v8/tools/tickprocessor.js',
      'deps/v8/tools/SourceMap.js',
      'deps/v8/tools/tickprocessor-driver.js',
      'deps/node-inspect/lib/_inspect.js',
      'deps/node-inspect/lib/internal/inspect_client.js',
      'deps/node-inspect/lib/internal/inspect_repl.js',
      'deps/acorn/acorn/dist/acorn.js',
      'deps/acorn/acorn-walk/dist/walk.js',
    ],
    'conditions': [
      [ 'node_shared=="true"', {
        'node_target_type%': 'shared_library',
        'conditions': [
          ['OS=="aix"', {
            # For AIX, always generate static library first,
            # It needs an extra step to generate exp and
            # then use both static lib and exp to create
            # shared lib.
            'node_intermediate_lib_type': 'static_library',
          }, {
            'node_intermediate_lib_type': 'shared_library',
          }],
        ],
      }, {
        'node_target_type%': 'executable',
      }],
      [ 'OS=="win" and '
        'node_use_openssl=="true" and '
        'node_shared_openssl=="false"', {
        'use_openssl_def%': 1,
      }, {
        'use_openssl_def%': 0,
      }],
    ],
  },

  'targets': [
    {
      'target_name': '<(node_core_target_name)',
      'type': 'executable',
      'sources': [
        'src/node_main.cc'
      ],
      'includes': [
        'node.gypi'
      ],
      'include_dirs': [
        'src',
        'deps/v8/include'
      ],
      'dependencies': [ 'deps/histogram/histogram.gyp:histogram' ],

      # - "C4244: conversion from 'type1' to 'type2', possible loss of data"
      #   Ususaly safe. Disable for `dep`, enable for `src`
      'msvs_disabled_warnings!': [4244],

      'conditions': [
        [ 'node_intermediate_lib_type=="static_library" and '
            'node_shared=="true" and OS=="aix"', {
          # For AIX, shared lib is linked by static lib and .exp. In the
          # case here, the executable needs to link to shared lib.
          # Therefore, use 'node_aix_shared' target to generate the
          # shared lib and then executable.
          'dependencies': [ 'node_aix_shared' ],
        }, {
          'dependencies': [ '<(node_lib_target_name)' ],
        }],
        [ 'node_intermediate_lib_type=="static_library" and '
            'node_shared=="false"', {
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-force_load,<(PRODUCT_DIR)/<(STATIC_LIB_PREFIX)'
                  '<(node_core_target_name)<(STATIC_LIB_SUFFIX)',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/WHOLEARCHIVE:<(node_core_target_name)<(STATIC_LIB_SUFFIX)',
              ],
            },
          },
          'conditions': [
            ['OS!="aix"', {
              'ldflags': [
                '-Wl,--whole-archive,<(obj_dir)/<(STATIC_LIB_PREFIX)'
                    '<(node_core_target_name)<(STATIC_LIB_SUFFIX)',
                '-Wl,--no-whole-archive',
              ],
            }],
            [ 'OS=="win"', {
              'sources': [ 'src/res/node.rc' ],
              'conditions': [
                [ 'node_use_etw=="true"', {
                  'sources': [
                    'tools/msvs/genfiles/node_etw_provider.rc'
                  ],
                }],
              ],
            }],
          ],
        }],
        [ 'node_shared=="true"', {
          'xcode_settings': {
            'OTHER_LDFLAGS': [ '-Wl,-rpath,@loader_path', ],
          },
        }],
        [ 'node_intermediate_lib_type=="shared_library" and OS=="win"', {
          # On Windows, having the same name for both executable and shared
          # lib causes filename collision. Need a different PRODUCT_NAME for
          # the executable and rename it back to node.exe later
          'product_name': '<(node_core_target_name)-win',
        }],
        [ 'node_report=="true"', {
          'defines': [
            'NODE_REPORT',
            'NODE_ARCH="<(target_arch)"',
            'NODE_PLATFORM="<(OS)"',
          ],
          'conditions': [
            ['OS=="win"', {
              'libraries': [
                'dbghelp.lib',
                'PsApi.lib',
                'Ws2_32.lib',
              ],
              'dll_files': [
                'dbghelp.dll',
                'PsApi.dll',
                'Ws2_32.dll',
              ],
            }],
          ],
        }],
      ],
    }, # node_core_target_name
    {
      'target_name': '<(node_lib_target_name)',
      'type': '<(node_intermediate_lib_type)',
      'product_name': '<(node_core_target_name)',
      'includes': [
        'node.gypi'
      ],

      'include_dirs': [
        'src',
        '<(SHARED_INTERMEDIATE_DIR)' # for node_natives.h
      ],
      'dependencies': [ 'deps/histogram/histogram.gyp:histogram' ],

      'sources': [
        'src/api/callback.cc',
        'src/api/encoding.cc',
        'src/api/environment.cc',
        'src/api/exceptions.cc',
        'src/api/hooks.cc',
        'src/api/utils.cc',

        'src/async_wrap.cc',
        'src/cares_wrap.cc',
        'src/connect_wrap.cc',
        'src/connection_wrap.cc',
        'src/debug_utils.cc',
        'src/env.cc',
        'src/fs_event_wrap.cc',
        'src/handle_wrap.cc',
        'src/heap_utils.cc',
        'src/js_native_api.h',
        'src/js_native_api_types.h',
        'src/js_native_api_v8.cc',
        'src/js_native_api_v8.h',
        'src/js_native_api_v8_internals.h',
        'src/js_stream.cc',
        'src/module_wrap.cc',
        'src/node.cc',
        'src/node_api.cc',
        'src/node_binding.cc',
        'src/node_buffer.cc',
        'src/node_config.cc',
        'src/node_constants.cc',
        'src/node_contextify.cc',
        'src/node_credentials.cc',
        'src/node_domain.cc',
        'src/node_env_var.cc',
        'src/node_errors.cc',
        'src/node_file.cc',
        'src/node_http_parser_llhttp.cc',
        'src/node_http_parser_traditional.cc',
        'src/node_http2.cc',
        'src/node_i18n.cc',
        'src/node_messaging.cc',
        'src/node_metadata.cc',
        'src/node_native_module.cc',
        'src/node_options.cc',
        'src/node_os.cc',
        'src/node_perf.cc',
        'src/node_platform.cc',
        'src/node_postmortem_metadata.cc',
        'src/node_process_events.cc',
        'src/node_process_methods.cc',
        'src/node_process_object.cc',
        'src/node_serdes.cc',
        'src/node_stat_watcher.cc',
        'src/node_symbols.cc',
        'src/node_task_queue.cc',
        'src/node_trace_events.cc',
        'src/node_types.cc',
        'src/node_url.cc',
        'src/node_util.cc',
        'src/node_v8.cc',
        'src/node_watchdog.cc',
        'src/node_worker.cc',
        'src/node_zlib.cc',
        'src/pipe_wrap.cc',
        'src/process_wrap.cc',
        'src/sharedarraybuffer_metadata.cc',
        'src/signal_wrap.cc',
        'src/spawn_sync.cc',
        'src/stream_base.cc',
        'src/stream_pipe.cc',
        'src/stream_wrap.cc',
        'src/string_bytes.cc',
        'src/string_decoder.cc',
        'src/tcp_wrap.cc',
        'src/timers.cc',
        'src/tracing/agent.cc',
        'src/tracing/node_trace_buffer.cc',
        'src/tracing/node_trace_writer.cc',
        'src/tracing/trace_event.cc',
        'src/tracing/traced_value.cc',
        'src/tty_wrap.cc',
        'src/udp_wrap.cc',
        'src/util.cc',
        'src/uv.cc',
        # headers to make for a more pleasant IDE experience
        'src/aliased_buffer.h',
        'src/async_wrap.h',
        'src/async_wrap-inl.h',
        'src/base_object.h',
        'src/base_object-inl.h',
        'src/base64.h',
        'src/connect_wrap.h',
        'src/connection_wrap.h',
        'src/debug_utils.h',
        'src/env.h',
        'src/env-inl.h',
        'src/handle_wrap.h',
        'src/histogram.h',
        'src/histogram-inl.h',
        'src/http_parser_adaptor.h',
        'src/js_stream.h',
        'src/memory_tracker.h',
        'src/memory_tracker-inl.h',
        'src/module_wrap.h',
        'src/node.h',
        'src/node_api.h',
        'src/node_api_types.h',
        'src/node_binding.h',
        'src/node_buffer.h',
        'src/node_constants.h',
        'src/node_context_data.h',
        'src/node_contextify.h',
        'src/node_errors.h',
        'src/node_file.h',
        'src/node_http_parser_impl.h',
        'src/node_http2.h',
        'src/node_http2_state.h',
        'src/node_i18n.h',
        'src/node_internals.h',
        'src/node_messaging.h',
        'src/node_metadata.h',
        'src/node_mutex.h',
        'src/node_native_module.h',
        'src/node_object_wrap.h',
        'src/node_options.h',
        'src/node_options-inl.h',
        'src/node_perf.h',
        'src/node_perf_common.h',
        'src/node_persistent.h',
        'src/node_platform.h',
        'src/node_process.h',
        'src/node_revert.h',
        'src/node_root_certs.h',
        'src/node_stat_watcher.h',
        'src/node_union_bytes.h',
        'src/node_url.h',
        'src/node_version.h',
        'src/node_v8_platform-inl.h',
        'src/node_watchdog.h',
        'src/node_worker.h',
        'src/pipe_wrap.h',
        'src/req_wrap.h',
        'src/req_wrap-inl.h',
        'src/sharedarraybuffer_metadata.h',
        'src/spawn_sync.h',
        'src/stream_base.h',
        'src/stream_base-inl.h',
        'src/stream_pipe.h',
        'src/stream_wrap.h',
        'src/string_bytes.h',
        'src/string_decoder.h',
        'src/string_decoder-inl.h',
        'src/string_search.h',
        'src/tcp_wrap.h',
        'src/tracing/agent.h',
        'src/tracing/node_trace_buffer.h',
        'src/tracing/node_trace_writer.h',
        'src/tracing/trace_event.h',
        'src/tracing/trace_event_common.h',
        'src/tracing/traced_value.h',
        'src/tty_wrap.h',
        'src/udp_wrap.h',
        'src/util.h',
        'src/util-inl.h',
        # Dependency headers
        'deps/http_parser/http_parser.h',
        'deps/v8/include/v8.h',
        # javascript files to make for an even more pleasant IDE experience
        '<@(library_files)',
        # node.gyp is added by default, common.gypi is added for change detection
        'common.gypi',
      ],

      'variables': {
        'openssl_system_ca_path%': '',
      },

      'defines': [
        'NODE_ARCH="<(target_arch)"',
        'NODE_PLATFORM="<(OS)"',
        'NODE_WANT_INTERNALS=1',
        # Warn when using deprecated V8 APIs.
        'V8_DEPRECATION_WARNINGS=1',
        'NODE_OPENSSL_SYSTEM_CERT_PATH="<(openssl_system_ca_path)"',
      ],

      # - "C4244: conversion from 'type1' to 'type2', possible loss of data"
      #   Ususaly safe. Disable for `dep`, enable for `src`
      'msvs_disabled_warnings!': [4244],

      'conditions': [
        [ 'node_code_cache_path!=""', {
          'sources': [ '<(node_code_cache_path)' ]
        }, {
          'sources': [ 'src/node_code_cache_stub.cc' ]
        }],
        [ 'node_shared=="true" and node_module_version!="" and OS!="win"', {
          'product_extension': '<(shlib_suffix)',
          'xcode_settings': {
            'LD_DYLIB_INSTALL_NAME':
              '@rpath/lib<(node_core_target_name).<(shlib_suffix)'
          },
        }],
        ['node_shared=="true" and OS=="aix"', {
          'product_name': 'node_base',
        }],
        [ 'v8_enable_inspector==1', {
          'includes' : [ 'src/inspector/node_inspector.gypi' ],
        }, {
          'defines': [ 'HAVE_INSPECTOR=0' ]
        }],
        [ 'OS=="win"', {
          'conditions': [
            [ 'node_intermediate_lib_type!="static_library"', {
              'sources': [
                'src/res/node.rc',
              ],
            }],
          ],
          'libraries': [ '-lpsapi.lib' ]
        }],
        [ 'node_use_etw=="true"', {
          'defines': [ 'HAVE_ETW=1' ],
          'dependencies': [ 'node_etw' ],
          'include_dirs': [
            'src',
            'tools/msvs/genfiles',
            '<(SHARED_INTERMEDIATE_DIR)' # for node_natives.h
          ],
          'sources': [
            'src/node_win32_etw_provider.h',
            'src/node_win32_etw_provider-inl.h',
            'src/node_win32_etw_provider.cc',
            'src/node_dtrace.h',
            'src/node_dtrace.cc',
            'tools/msvs/genfiles/node_etw_provider.h',
          ],
          'conditions': [
            ['node_intermediate_lib_type != "static_library"', {
              'sources': [
                'tools/msvs/genfiles/node_etw_provider.rc',
              ],
            }],
          ],
        }],
        [ 'node_use_dtrace=="true"', {
          'defines': [ 'HAVE_DTRACE=1' ],
          'dependencies': [
            'node_dtrace_header',
            'specialize_node_d',
          ],
          'include_dirs': [ '<(SHARED_INTERMEDIATE_DIR)' ],
          #
          # DTrace is supported on linux, solaris, mac, and bsd.  There are
          # three object files associated with DTrace support, but they're
          # not all used all the time:
          #
          #   node_dtrace.o           all configurations
          #   node_dtrace_ustack.o    not supported on mac and linux
          #   node_dtrace_provider.o  All except OS X.  "dtrace -G" is not
          #                           used on OS X.
          #
          # Note that node_dtrace_provider.cc and node_dtrace_ustack.cc do not
          # actually exist.  They're listed here to trick GYP into linking the
          # corresponding object files into the final "node" executable.  These
          # object files are generated by "dtrace -G" using custom actions
          # below, and the GYP-generated Makefiles will properly build them when
          # needed.
          #
          'sources': [
            'src/node_dtrace.h',
            'src/node_dtrace.cc',
          ],
          'conditions': [
            [ 'OS=="linux"', {
              'sources': [
                '<(SHARED_INTERMEDIATE_DIR)/node_dtrace_provider.o'
              ],
            }],
            [ 'OS!="mac" and OS!="linux"', {
              'sources': [
                'src/node_dtrace_ustack.cc',
                'src/node_dtrace_provider.cc',
              ]
            }
          ] ]
        } ],
        [ 'node_use_openssl=="true"', {
          'sources': [
            'src/node_crypto.cc',
            'src/node_crypto_bio.cc',
            'src/node_crypto_clienthello.cc',
            'src/node_crypto.h',
            'src/node_crypto_bio.h',
            'src/node_crypto_clienthello.h',
            'src/node_crypto_clienthello-inl.h',
            'src/node_crypto_groups.h',
            'src/tls_wrap.cc',
            'src/tls_wrap.h'
          ],
        }],
        [ 'node_report=="true"', {
          'sources': [
            'src/node_report.cc',
            'src/node_report_module.cc',
            'src/node_report_utils.cc',
          ],
          'defines': [
            'NODE_REPORT',
            'NODE_ARCH="<(target_arch)"',
            'NODE_PLATFORM="<(OS)"',
          ],
          'conditions': [
            ['OS=="win"', {
              'libraries': [
                'dbghelp.lib',
                'PsApi.lib',
                'Ws2_32.lib',
              ],
              'dll_files': [
                'dbghelp.dll',
                'PsApi.dll',
                'Ws2_32.dll',
              ],
            }],
          ],
        }],
        [ 'node_use_large_pages=="true" and OS=="linux"', {
          'defines': [ 'NODE_ENABLE_LARGE_CODE_PAGES=1' ],
          # The current implementation of Large Pages is under Linux.
          # Other implementations are possible but not currently supported.
          'sources': [
            'src/large_pages/node_large_page.cc',
            'src/large_pages/node_large_page.h'
          ],
        }],
        [ 'use_openssl_def==1', {
          # TODO(bnoordhuis) Make all platforms export the same list of symbols.
          # Teach mkssldef.py to generate linker maps that UNIX linkers understand.
          'variables': {
            'mkssldef_flags': [
              # Categories to export.
              '-CAES,BF,BIO,DES,DH,DSA,EC,ECDH,ECDSA,ENGINE,EVP,HMAC,MD4,MD5,'
              'PSK,RC2,RC4,RSA,SHA,SHA0,SHA1,SHA256,SHA512,SOCK,STDIO,TLSEXT,'
              'FP_API,TLS1_METHOD,TLS1_1_METHOD,TLS1_2_METHOD,SCRYPT,OCSP,'
              'NEXTPROTONEG,RMD160,CAST,DEPRECATEDIN_1_1_0,DEPRECATEDIN_1_2_0',
              # Defines.
              '-DWIN32',
              # Symbols to filter from the export list.
              '-X^DSO',
              '-X^_',
              '-X^private_',
              # Base generated DEF on zlib.def
              '-Bdeps/zlib/win32/zlib.def'
            ],
          },
          'conditions': [
            ['openssl_fips!=""', {
              'variables': { 'mkssldef_flags': ['-DOPENSSL_FIPS'] },
            }],
          ],
          'actions': [
            {
              'action_name': 'mkssldef',
              'inputs': [
                'deps/openssl/openssl/util/libcrypto.num',
                'deps/openssl/openssl/util/libssl.num',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/openssl.def'],
              'process_outputs_as_sources': 1,
              'action': [
                'python',
                'tools/mkssldef.py',
                '<@(mkssldef_flags)',
                '-o',
                '<@(_outputs)',
                '<@(_inputs)',
              ],
            },
          ],
        }],
      ],
      'actions': [
        {
          'action_name': 'node_js2c',
          'process_outputs_as_sources': 1,
          'inputs': [
            '<@(library_files)',
            'config.gypi',
            'tools/check_macros.py'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/node_javascript.cc',
          ],
          'conditions': [
            [ 'node_use_dtrace=="false" and node_use_etw=="false"', {
              'inputs': [ 'src/notrace_macros.py' ]
            }],
            [ 'node_debug_lib=="false"', {
              'inputs': [ 'tools/nodcheck_macros.py' ]
            }],
            [ 'node_debug_lib=="true"', {
              'inputs': [ 'tools/dcheck_macros.py' ]
            }]
          ],
          'action': [
            'python', 'tools/js2c.py',
            '<@(_outputs)',
            '<@(_inputs)',
          ],
        },
      ],
    }, # node_lib_target_name
    {
       # generate ETW header and resource files
      'target_name': 'node_etw',
      'type': 'none',
      'conditions': [
        [ 'node_use_etw=="true"', {
          'actions': [
            {
              'action_name': 'node_etw',
              'inputs': [ 'src/res/node_etw_provider.man' ],
              'outputs': [
                'tools/msvs/genfiles/node_etw_provider.rc',
                'tools/msvs/genfiles/node_etw_provider.h',
                'tools/msvs/genfiles/node_etw_providerTEMP.BIN',
              ],
              'action': [ 'mc <@(_inputs) -h tools/msvs/genfiles -r tools/msvs/genfiles' ]
            }
          ]
        } ]
      ]
    }, # node_etw
    {
      'target_name': 'node_dtrace_header',
      'type': 'none',
      'conditions': [
        [ 'node_use_dtrace=="true" and OS!="linux"', {
          'actions': [
            {
              'action_name': 'node_dtrace_header',
              'inputs': [ 'src/node_provider.d' ],
              'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/node_provider.h' ],
              'action': [ 'dtrace', '-h', '-xnolibs', '-s', '<@(_inputs)',
                '-o', '<@(_outputs)' ]
            }
          ]
        } ],
        [ 'node_use_dtrace=="true" and OS=="linux"', {
          'actions': [
            {
              'action_name': 'node_dtrace_header',
              'inputs': [ 'src/node_provider.d' ],
              'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/node_provider.h' ],
              'action': [ 'dtrace', '-h', '-s', '<@(_inputs)',
                '-o', '<@(_outputs)' ]
            }
          ]
        } ],
      ]
    }, # node_dtrace_header
    {
      'target_name': 'node_dtrace_provider',
      'type': 'none',
      'conditions': [
        [ 'node_use_dtrace=="true" and OS!="mac" and OS!="linux"', {
          'actions': [
            {
              'action_name': 'node_dtrace_provider_o',
              'inputs': [
                '<(obj_dir)/<(node_lib_target_name)/src/node_dtrace.o',
              ],
              'outputs': [
                '<(obj_dir)/<(node_lib_target_name)/src/node_dtrace_provider.o'
              ],
              'action': [ 'dtrace', '-G', '-xnolibs', '-s', 'src/node_provider.d',
                '<@(_inputs)', '-o', '<@(_outputs)' ]
            }
          ]
        }],
        [ 'node_use_dtrace=="true" and OS=="linux"', {
          'actions': [
            {
              'action_name': 'node_dtrace_provider_o',
              'inputs': [ 'src/node_provider.d' ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/node_dtrace_provider.o'
              ],
              'action': [
                'dtrace', '-C', '-G', '-s', '<@(_inputs)', '-o', '<@(_outputs)'
              ],
            }
          ],
        }],
      ]
    }, # node_dtrace_provider
    {
      'target_name': 'node_dtrace_ustack',
      'type': 'none',
      'conditions': [
        [ 'node_use_dtrace=="true" and OS!="mac" and OS!="linux"', {
          'actions': [
            {
              'action_name': 'node_dtrace_ustack_constants',
              'inputs': [
                '<(v8_base)'
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/v8constants.h'
              ],
              'action': [
                'tools/genv8constants.py',
                '<@(_outputs)',
                '<@(_inputs)'
              ]
            },
            {
              'action_name': 'node_dtrace_ustack',
              'inputs': [
                'src/v8ustack.d',
                '<(SHARED_INTERMEDIATE_DIR)/v8constants.h'
              ],
              'outputs': [
                '<(obj_dir)/<(node_lib_target_name)/src/node_dtrace_ustack.o'
              ],
              'conditions': [
                [ 'target_arch=="ia32" or target_arch=="arm"', {
                  'action': [
                    'dtrace', '-32', '-I<(SHARED_INTERMEDIATE_DIR)', '-Isrc',
                    '-C', '-G', '-s', 'src/v8ustack.d', '-o', '<@(_outputs)',
                  ]
                } ],
                [ 'target_arch=="x64"', {
                  'action': [
                    'dtrace', '-64', '-I<(SHARED_INTERMEDIATE_DIR)', '-Isrc',
                    '-C', '-G', '-s', 'src/v8ustack.d', '-o', '<@(_outputs)',
                  ]
                } ],
              ]
            },
          ]
        } ],
      ]
    }, # node_dtrace_ustack
    {
      'target_name': 'specialize_node_d',
      'type': 'none',
      'conditions': [
        [ 'node_use_dtrace=="true"', {
          'actions': [
            {
              'action_name': 'specialize_node_d',
              'inputs': [
                'src/node.d'
              ],
              'outputs': [
                '<(PRODUCT_DIR)/node.d',
              ],
              'action': [
                'tools/specialize_node_d.py',
                '<@(_outputs)',
                '<@(_inputs)',
                '<@(OS)',
                '<@(target_arch)',
              ],
            },
          ],
        } ],
      ]
    }, # specialize_node_d
    {
      # When using shared lib to build executable in Windows, in order to avoid
      # filename collision, the executable name is node-win.exe. Need to rename
      # it back to node.exe
      'target_name': 'rename_node_bin_win',
      'type': 'none',
      'dependencies': [
        '<(node_core_target_name)',
      ],
      'conditions': [
        [ 'OS=="win" and node_intermediate_lib_type=="shared_library"', {
          'actions': [
            {
              'action_name': 'rename_node_bin_win',
              'inputs': [
                '<(PRODUCT_DIR)/<(node_core_target_name)-win.exe'
              ],
              'outputs': [
                '<(PRODUCT_DIR)/<(node_core_target_name).exe',
              ],
              'action': [
                'move', '<@(_inputs)', '<@(_outputs)',
              ],
            },
          ],
        } ],
      ]
    }, # rename_node_bin_win
    {
      'target_name': 'cctest',
      'type': 'executable',

      'dependencies': [
        '<(node_lib_target_name)',
        'rename_node_bin_win',
        'deps/gtest/gtest.gyp:gtest',
        'deps/histogram/histogram.gyp:histogram',
        'node_dtrace_header',
        'node_dtrace_ustack',
        'node_dtrace_provider',
      ],

      'includes': [
        'node.gypi'
      ],

      'include_dirs': [
        'src',
        'tools/msvs/genfiles',
        'deps/v8/include',
        'deps/cares/include',
        'deps/uv/include',
        '<(SHARED_INTERMEDIATE_DIR)', # for node_natives.h
      ],

      'defines': [ 'NODE_WANT_INTERNALS=1' ],

      'sources': [
        'test/cctest/node_test_fixture.cc',
        'test/cctest/test_aliased_buffer.cc',
        'test/cctest/test_base64.cc',
        'test/cctest/test_node_postmortem_metadata.cc',
        'test/cctest/test_environment.cc',
        'test/cctest/test_platform.cc',
        'test/cctest/test_report_util.cc',
        'test/cctest/test_traced_value.cc',
        'test/cctest/test_util.cc',
        'test/cctest/test_url.cc'
      ],

      'conditions': [
        [ 'node_use_openssl=="true"', {
          'defines': [
            'HAVE_OPENSSL=1',
          ],
        }],
        ['v8_enable_inspector==1', {
          'sources': [
            'test/cctest/test_inspector_socket.cc',
            'test/cctest/test_inspector_socket_server.cc'
          ],
          'defines': [
            'HAVE_INSPECTOR=1',
          ],
        }, {
          'defines': [ 'HAVE_INSPECTOR=0' ]
        }],
        ['OS=="solaris"', {
          'ldflags': [ '-I<(SHARED_INTERMEDIATE_DIR)' ]
        }],
        # Skip cctest while building shared lib node for Windows
        [ 'OS=="win" and node_shared=="true"', {
          'type': 'none',
        }],
        [ 'node_shared=="true"', {
          'xcode_settings': {
            'OTHER_LDFLAGS': [ '-Wl,-rpath,@loader_path', ],
          },
        }],
        [ 'node_report=="true"', {
          'defines': [
            'NODE_REPORT',
            'NODE_ARCH="<(target_arch)"',
            'NODE_PLATFORM="<(OS)"',
          ],
          'conditions': [
            ['OS=="win"', {
              'libraries': [
                'dbghelp.lib',
                'PsApi.lib',
                'Ws2_32.lib',
              ],
              'dll_files': [
                'dbghelp.dll',
                'PsApi.dll',
                'Ws2_32.dll',
              ],
            }],
          ],
        }],
      ],
    }, # cctest
  ], # end targets

  'conditions': [
    [ 'OS=="aix" and node_shared=="true"', {
      'targets': [
        {
          'target_name': 'node_aix_shared',
          'type': 'shared_library',
          'product_name': '<(node_core_target_name)',
          'ldflags': [ '--shared' ],
          'product_extension': '<(shlib_suffix)',
          'includes': [
            'node.gypi'
          ],
          'dependencies': [ '<(node_lib_target_name)' ],
          'include_dirs': [
            'src',
            'deps/v8/include',
          ],
          'sources': [
            '<@(library_files)',
            'common.gypi',
          ],
          'direct_dependent_settings': {
            'ldflags': [ '-Wl,-brtl' ],
          },
        },
      ]
    }], # end aix section
  ], # end conditions block
}
