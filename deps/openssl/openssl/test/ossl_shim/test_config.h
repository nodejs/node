/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_TEST_SHIM_TEST_CONFIG_H
#define OSSL_TEST_SHIM_TEST_CONFIG_H

#include <string>
#include <vector>


struct TestConfig {
  int port = 0;
  bool is_server = false;
  bool is_dtls = false;
  int resume_count = 0;
  bool fallback_scsv = false;
  std::string key_file;
  std::string cert_file;
  std::string expected_server_name;
  std::string expected_certificate_types;
  bool require_any_client_certificate = false;
  std::string advertise_npn;
  std::string expected_next_proto;
  std::string select_next_proto;
  bool async = false;
  bool write_different_record_sizes = false;
  bool partial_write = false;
  bool no_tls13 = false;
  bool no_tls12 = false;
  bool no_tls11 = false;
  bool no_tls1 = false;
  bool no_ssl3 = false;
  bool shim_writes_first = false;
  std::string host_name;
  std::string advertise_alpn;
  std::string expected_alpn;
  std::string expected_advertised_alpn;
  std::string select_alpn;
  bool decline_alpn = false;
  bool expect_session_miss = false;
  bool expect_extended_master_secret = false;
  std::string psk;
  std::string psk_identity;
  std::string srtp_profiles;
  int min_version = 0;
  int max_version = 0;
  int mtu = 0;
  bool implicit_handshake = false;
  std::string cipher;
  bool handshake_never_done = false;
  int export_keying_material = 0;
  std::string export_label;
  std::string export_context;
  bool use_export_context = false;
  bool expect_ticket_renewal = false;
  bool expect_no_session = false;
  bool use_ticket_callback = false;
  bool renew_ticket = false;
  bool enable_client_custom_extension = false;
  bool enable_server_custom_extension = false;
  bool custom_extension_skip = false;
  bool custom_extension_fail_add = false;
  bool check_close_notify = false;
  bool shim_shuts_down = false;
  bool verify_fail = false;
  bool verify_peer = false;
  bool expect_verify_result = false;
  int expect_total_renegotiations = 0;
  bool renegotiate_freely = false;
  bool p384_only = false;
  bool enable_all_curves = false;
  bool use_sparse_dh_prime = false;
  bool use_old_client_cert_callback = false;
  bool use_null_client_ca_list = false;
  bool peek_then_read = false;
  int max_cert_list = 0;
};

bool ParseConfig(int argc, char **argv, TestConfig *out_config);


#endif  // OSSL_TEST_SHIM_TEST_CONFIG_H
