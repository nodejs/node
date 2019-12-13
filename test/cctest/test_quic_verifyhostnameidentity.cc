
#include "base_object-inl.h"
#include "node_quic_crypto.h"
#include "node_quic_util-inl.h"
#include "node_sockaddr-inl.h"
#include "util.h"
#include "gtest/gtest.h"

#include <openssl/ssl.h>

#include <string>
#include <unordered_map>
#include <vector>

using node::quic::VerifyHostnameIdentity;

enum altname_type {
  TYPE_DNS,
  TYPE_IP,
  TYPE_URI
};

struct altname {
  altname_type type;
  const char* name;
};

void ToAltNamesMap(
    const altname* names,
    size_t names_len,
    std::unordered_multimap<std::string, std::string>* map) {
  for (size_t n = 0; n < names_len; n++) {
    switch (names[n].type) {
      case TYPE_DNS:
        map->emplace("dns", names[n].name);
        continue;
      case TYPE_IP:
        map->emplace("ip", names[n].name);
        continue;
      case TYPE_URI:
        map->emplace("uri", names[n].name);
        continue;
    }
  }
}

TEST(QuicCrypto, BasicCN_1) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(VerifyHostnameIdentity("a.com", std::string("a.com"), altnames), 0);
}

TEST(QuicCrypto, BasicCN_2) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(VerifyHostnameIdentity("a.com", std::string("A.com"), altnames), 0);
}

TEST(QuicCrypto, BasicCN_3_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(
      VerifyHostnameIdentity("a.com", std::string("b.com"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_4) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(VerifyHostnameIdentity("a.com", std::string("a.com."), altnames), 0);
}

TEST(QuicCrypto, BasicCN_5_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(
      VerifyHostnameIdentity("a.com", std::string(".a.com"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_6_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(
      VerifyHostnameIdentity("8.8.8.8", std::string("8.8.8.8"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_7_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "8.8.8.8");
  CHECK_EQ(
      VerifyHostnameIdentity("8.8.8.8", std::string("8.8.8.8"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_8_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("uri", "8.8.8.8");
  CHECK_EQ(
      VerifyHostnameIdentity("8.8.8.8", std::string("8.8.8.8"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_9) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("ip", "8.8.8.8");
  CHECK_EQ(
      VerifyHostnameIdentity("8.8.8.8", std::string("8.8.8.8"), altnames), 0);
}

TEST(QuicCrypto, BasicCN_10_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("ip", "8.8.8.8/24");
  CHECK_EQ(
      VerifyHostnameIdentity("8.8.8.8", std::string("8.8.8.8"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_11) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(
      VerifyHostnameIdentity("b.a.com", std::string("*.a.com"), altnames), 0);
}

TEST(QuicCrypto, BasicCN_12_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(
      VerifyHostnameIdentity("ba.com", std::string("*.a.com"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_13_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(
      VerifyHostnameIdentity("\n.a.com", std::string("*.a.com"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_14_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "omg.com");
  CHECK_EQ(
      VerifyHostnameIdentity("b.a.com", std::string("*.a.com"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_15_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(
      VerifyHostnameIdentity("b.a.com", std::string("b*b.a.com"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_16_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(
      VerifyHostnameIdentity("b.a.com", std::string(), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_17) {
  // TODO(@jasnell): This should test multiple CN's. The code is only
  // implemented to support one. Need to fix
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(
      VerifyHostnameIdentity("foo.com", std::string("foo.com"), altnames), 0);
}

TEST(QuicCrypto, BasicCN_18_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*");
  CHECK_EQ(
      VerifyHostnameIdentity("a.com", std::string("b.com"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_19_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*.com");
  CHECK_EQ(
      VerifyHostnameIdentity("a.com", std::string("b.com"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_20) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*.co.uk");
  CHECK_EQ(
      VerifyHostnameIdentity("a.co.uk", std::string("b.com"), altnames), 0);
}

TEST(QuicCrypto, BasicCN_21_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*.a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("a.com", std::string("a.com"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_22_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*.a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("a.com", std::string("b.com"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_23) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("a.com", std::string("b.com"), altnames), 0);
}

TEST(QuicCrypto, BasicCN_24) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "A.COM");
  CHECK_EQ(
      VerifyHostnameIdentity("a.com", std::string("b.com"), altnames), 0);
}

TEST(QuicCrypto, BasicCN_25_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*.a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("a.com", std::string(), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_26) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*.a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("b.a.com", std::string(), altnames), 0);
}

TEST(QuicCrypto, BasicCN_27_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*.a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("c.b.a.com", std::string(), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_28) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*b.a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("b.a.com", std::string(), altnames), 0);
}

TEST(QuicCrypto, BasicCN_29) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*b.a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("a-cb.a.com", std::string(), altnames), 0);
}

TEST(QuicCrypto, BasicCN_30_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*b.a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("a.b.a.com", std::string(), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}


TEST(QuicCrypto, BasicCN_31) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "*b.a.com");
  altnames.emplace("dns", "a.b.a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("a.b.a.com", std::string(), altnames), 0);
}


TEST(QuicCrypto, BasicCN_32) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("uri", "a.b.a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("a.b.a.com", std::string(), altnames), 0);
}

TEST(QuicCrypto, BasicCN_33_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("uri", "*.b.a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("a.b.a.com", std::string(), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

//   // Invalid URI
//   {
//     host: 'a.b.a.com', cert: {
//       subjectaltname: 'URI:http://[a.b.a.com]/',
//       subject: {}
//     }
//   },

TEST(QuicCrypto, BasicCN_35_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("ip", "127.0.0.1");
  CHECK_EQ(
      VerifyHostnameIdentity("a.b.a.com", std::string(), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_36) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("ip", "127.0.0.1");
  CHECK_EQ(
      VerifyHostnameIdentity("127.0.0.1", std::string(), altnames), 0);
}

TEST(QuicCrypto, BasicCN_37_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("ip", "127.0.0.1");
  CHECK_EQ(
      VerifyHostnameIdentity("127.0.0.2", std::string(), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_38_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("127.0.0.1", std::string(), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_39_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  altnames.emplace("dns", "a.com");
  CHECK_EQ(
      VerifyHostnameIdentity("localhost", std::string("localhost"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(QuicCrypto, BasicCN_40) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(
      VerifyHostnameIdentity(
          "xn--bcher-kva.example.com",
          std::string("*.example.com"), altnames), 0);
}

TEST(QuicCrypto, BasicCN_41_Fail) {
  std::unordered_multimap<std::string, std::string> altnames;
  CHECK_EQ(
      VerifyHostnameIdentity(
          "xn--bcher-kva.example.com",
          std::string("xn--*.example.com"), altnames),
      X509_V_ERR_HOSTNAME_MISMATCH);
}
