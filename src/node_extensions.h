
NODE_EXT_LIST_START
NODE_EXT_LIST_ITEM(node_buffer)
NODE_EXT_LIST_ITEM(node_cares)
#ifdef __POSIX__
NODE_EXT_LIST_ITEM(node_child_process)
#endif
#ifdef HAVE_OPENSSL
NODE_EXT_LIST_ITEM(node_crypto)
#endif
NODE_EXT_LIST_ITEM(node_evals)
NODE_EXT_LIST_ITEM(node_fs)
NODE_EXT_LIST_ITEM(node_net)
NODE_EXT_LIST_ITEM(node_http_parser)
NODE_EXT_LIST_ITEM(node_signal_watcher)
NODE_EXT_LIST_ITEM(node_stdio)
NODE_EXT_LIST_ITEM(node_os)
NODE_EXT_LIST_END

