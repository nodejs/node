
CSOURCES = ares__close_sockets.c	\
  ares__get_hostent.c			\
  ares__read_line.c			\
  ares__timeval.c			\
  ares_cancel.c				\
  ares_data.c				\
  ares_destroy.c			\
  ares_expand_name.c			\
  ares_expand_string.c			\
  ares_fds.c				\
  ares_free_hostent.c			\
  ares_free_string.c			\
  ares_gethostbyaddr.c			\
  ares_gethostbyname.c			\
  ares_getnameinfo.c			\
  ares_getsock.c			\
  ares_init.c				\
  ares_library_init.c			\
  ares_llist.c				\
  ares_mkquery.c			\
  ares_nowarn.c				\
  ares_options.c			\
  ares_parse_a_reply.c			\
  ares_parse_aaaa_reply.c		\
  ares_parse_mx_reply.c			\
  ares_parse_ns_reply.c			\
  ares_parse_ptr_reply.c		\
  ares_parse_srv_reply.c		\
  ares_parse_txt_reply.c		\
  ares_process.c			\
  ares_query.c				\
  ares_search.c				\
  ares_send.c				\
  ares_strcasecmp.c			\
  ares_strdup.c				\
  ares_strerror.c			\
  ares_timeout.c			\
  ares_version.c			\
  ares_writev.c				\
  bitncmp.c				\
  inet_net_pton.c			\
  inet_ntop.c				\
  windows_port.c

HHEADERS = ares.h			\
  ares_build.h				\
  ares_data.h				\
  ares_dns.h				\
  ares_ipv6.h				\
  ares_library_init.h			\
  ares_llist.h				\
  ares_nowarn.h				\
  ares_private.h			\
  ares_rules.h				\
  ares_strcasecmp.h			\
  ares_strdup.h				\
  ares_version.h			\
  ares_writev.h				\
  bitncmp.h				\
  inet_net_pton.h			\
  inet_ntop.h				\
  nameser.h				\
  ares_setup.h				\
  setup_once.h

MANPAGES = ares_cancel.3		\
  ares_destroy.3			\
  ares_destroy_options.3		\
  ares_dup.3				\
  ares_expand_name.3			\
  ares_expand_string.3			\
  ares_fds.3				\
  ares_free_data.3			\
  ares_free_hostent.3			\
  ares_free_string.3			\
  ares_get_servers.3			\
  ares_gethostbyaddr.3			\
  ares_gethostbyname.3			\
  ares_gethostbyname_file.3		\
  ares_getnameinfo.3			\
  ares_getsock.3			\
  ares_init.3				\
  ares_init_options.3			\
  ares_library_cleanup.3		\
  ares_library_init.3			\
  ares_mkquery.3			\
  ares_parse_a_reply.3			\
  ares_parse_aaaa_reply.3		\
  ares_parse_mx_reply.3			\
  ares_parse_ns_reply.3			\
  ares_parse_ptr_reply.3		\
  ares_parse_srv_reply.3		\
  ares_parse_txt_reply.3		\
  ares_process.3			\
  ares_query.3				\
  ares_save_options.3			\
  ares_search.3				\
  ares_send.3				\
  ares_set_servers.3			\
  ares_set_socket_callback.3		\
  ares_strerror.3			\
  ares_timeout.3			\
  ares_version.3

HTMLPAGES = ares_cancel.html		\
  ares_destroy.html			\
  ares_destroy_options.html		\
  ares_dup.html				\
  ares_expand_name.html			\
  ares_expand_string.html		\
  ares_fds.html				\
  ares_free_data.html			\
  ares_free_hostent.html		\
  ares_free_string.html			\
  ares_get_servers.html			\
  ares_gethostbyaddr.html		\
  ares_gethostbyname.html		\
  ares_gethostbyname_file.html		\
  ares_getnameinfo.html			\
  ares_getsock.html			\
  ares_init.html			\
  ares_init_options.html		\
  ares_library_cleanup.html		\
  ares_library_init.html		\
  ares_mkquery.html			\
  ares_parse_a_reply.html		\
  ares_parse_aaaa_reply.html		\
  ares_parse_mx_reply.html		\
  ares_parse_ns_reply.html		\
  ares_parse_ptr_reply.html		\
  ares_parse_srv_reply.html		\
  ares_parse_txt_reply.html		\
  ares_process.html			\
  ares_query.html			\
  ares_save_options.html		\
  ares_search.html			\
  ares_send.html			\
  ares_set_servers.html			\
  ares_set_socket_callback.html		\
  ares_strerror.html			\
  ares_timeout.html			\
  ares_version.html

PDFPAGES = ares_cancel.pdf		\
  ares_destroy.pdf			\
  ares_destroy_options.pdf		\
  ares_dup.pdf				\
  ares_expand_name.pdf			\
  ares_expand_string.pdf		\
  ares_fds.pdf				\
  ares_free_data.pdf			\
  ares_free_hostent.pdf			\
  ares_free_string.pdf			\
  ares_get_servers.pdf			\
  ares_gethostbyaddr.pdf		\
  ares_gethostbyname.pdf		\
  ares_gethostbyname_file.pdf		\
  ares_getnameinfo.pdf			\
  ares_getsock.pdf			\
  ares_init.pdf				\
  ares_init_options.pdf			\
  ares_library_cleanup.pdf		\
  ares_library_init.pdf			\
  ares_mkquery.pdf			\
  ares_parse_a_reply.pdf		\
  ares_parse_aaaa_reply.pdf		\
  ares_parse_mx_reply.pdf		\
  ares_parse_ns_reply.pdf		\
  ares_parse_ptr_reply.pdf		\
  ares_parse_srv_reply.pdf		\
  ares_parse_txt_reply.pdf		\
  ares_process.pdf			\
  ares_query.pdf			\
  ares_save_options.pdf			\
  ares_search.pdf			\
  ares_send.pdf				\
  ares_set_servers.pdf			\
  ares_set_socket_callback.pdf		\
  ares_strerror.pdf			\
  ares_timeout.pdf			\
  ares_version.pdf

SAMPLESOURCES = ares_getopt.c		\
  ares_strcasecmp.c			\
  inet_net_pton.c			\
  inet_ntop.c

SAMPLEHEADERS = ares_getopt.h		\
  ares_strcasecmp.h			\
  inet_net_pton.h			\
  inet_ntop.h
