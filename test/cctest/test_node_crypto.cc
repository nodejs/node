// This simulates specifying the configuration option --openssl-system-ca-path
// and setting it to a file that does not exist.
#define NODE_OPENSSL_SYSTEM_CERT_PATH "/missing/ca.pem"

#include "crypto/crypto_cipher.h"
#include "crypto/crypto_context.h"
#include "gtest/gtest.h"
#include "ncrypto.h"
#include "node_options.h"
#include "openssl/err.h"
#include "openssl/x509v3.h"

#include <climits>
#include <memory>
#include <string>

namespace {

struct GeneralNamesDeleter {
  void operator()(GENERAL_NAMES* names) const {
    sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
  }
};

struct AccessDescriptionsDeleter {
  void operator()(AUTHORITY_INFO_ACCESS* descriptions) const {
    sk_ACCESS_DESCRIPTION_pop_free(descriptions, ACCESS_DESCRIPTION_free);
  }
};

using GeneralNamePointer =
    ncrypto::DeleteFnPtr<GENERAL_NAME, GENERAL_NAME_free>;
using GeneralNamesPointer = std::unique_ptr<GENERAL_NAMES, GeneralNamesDeleter>;
using AccessDescriptionPointer =
    ncrypto::DeleteFnPtr<ACCESS_DESCRIPTION, ACCESS_DESCRIPTION_free>;
using AccessDescriptionsPointer =
    std::unique_ptr<AUTHORITY_INFO_ACCESS, AccessDescriptionsDeleter>;
using ASN1ObjectPointer = ncrypto::DeleteFnPtr<ASN1_OBJECT, ASN1_OBJECT_free>;
using ASN1StringPointer =
    ncrypto::DeleteFnPtr<ASN1_IA5STRING, ASN1_IA5STRING_free>;
using ASN1TimePointer = ncrypto::DeleteFnPtr<ASN1_TIME, ASN1_TIME_free>;
using X509ExtensionPointer =
    ncrypto::DeleteFnPtr<X509_EXTENSION, X509_EXTENSION_free>;
using X509NamePointer = ncrypto::DeleteFnPtr<X509_NAME, X509_NAME_free>;

std::string LongObjectIdentifier() {
  std::string oid = "1.2";
  for (int i = 0; i < 30; i++) oid += ".123456789";
  return oid;
}

std::string BioContents(const ncrypto::BIOPointer& bio) {
  BUF_MEM* mem = bio;
  if (mem == nullptr) return {};
  return std::string(mem->data, mem->length);
}

}  // namespace

/*
 * This test verifies that a call to NewRootCertDir with the build time
 * configuration option --openssl-system-ca-path set to an missing file, will
 * not leave any OpenSSL errors on the OpenSSL error stack.
 * See https://github.com/nodejs/node/issues/35456 for details.
 */
TEST(NodeCrypto, NewRootCertStore) {
  node::per_process::cli_options->ssl_openssl_cert_store = true;
  X509_STORE* store = node::crypto::NewRootCertStore();
  ASSERT_TRUE(store);
  ASSERT_EQ(ERR_peek_error(), 0UL) << "NewRootCertStore should not have left "
                                      "any errors on the OpenSSL error stack\n";
  X509_STORE_free(store);
}

TEST(NodeCrypto, TryGetIntCipherOutputLength) {
  int output_len = 0;

  EXPECT_TRUE(
      node::crypto::TryGetIntCipherOutputLength(INT_MAX - 16, 16, &output_len));
  EXPECT_EQ(output_len, INT_MAX);

  EXPECT_FALSE(
      node::crypto::TryGetIntCipherOutputLength(INT_MAX - 15, 16, &output_len));

  EXPECT_FALSE(node::crypto::TryGetIntCipherOutputLength(
      0, static_cast<size_t>(INT_MAX) + 1, &output_len));
}

TEST(NodeCrypto, BIOPointerRequiresCompleteWrites) {
  BIO* raw_output = nullptr;
  BIO* raw_input = nullptr;
  ASSERT_EQ(BIO_new_bio_pair(&raw_output, 1, &raw_input, 1), 1);
  ncrypto::BIOPointer output(raw_output);
  ncrypto::BIOPointer input(raw_input);
  EXPECT_FALSE(ncrypto::BIOPointer::WriteAll(output, "ab"));

  raw_output = nullptr;
  raw_input = nullptr;
  ASSERT_EQ(BIO_new_bio_pair(&raw_output, 1, &raw_input, 1), 1);
  output.reset(raw_output);
  input.reset(raw_input);
  EXPECT_FALSE(ncrypto::BIOPointer::PrintfAll(output, "%s", "ab"));
}

TEST(NodeCrypto, PreservesInvalidX509TimeDiagnostic) {
  ncrypto::X509Pointer cert(X509_new());
  ASN1TimePointer invalid_time(ASN1_STRING_type_new(V_ASN1_UTCTIME));
  ASSERT_TRUE(cert);
  ASSERT_TRUE(invalid_time);
  ASSERT_EQ(ASN1_STRING_set(invalid_time.get(), "221303214037Z", -1), 1);
  ASSERT_EQ(X509_set1_notBefore(cert.get(), invalid_time.get()), 1);

  const ncrypto::BIOPointer valid_from = cert.view().getValidFrom();
  ASSERT_TRUE(valid_from);
  EXPECT_EQ(BioContents(valid_from), "Bad time value");
  EXPECT_FALSE(cert.view().getValidFromTime().has_value());
}

#ifdef OPENSSL_IS_BORINGSSL
TEST(NodeCrypto, PreservesBoringSSLTimezoneOffsetDiagnostic) {
  ncrypto::X509Pointer cert(X509_new());
  ASN1TimePointer offset_time(ASN1_STRING_type_new(V_ASN1_UTCTIME));
  ASSERT_TRUE(cert);
  ASSERT_TRUE(offset_time);
  ASSERT_EQ(ASN1_STRING_set(offset_time.get(), "700101000000+0400", -1), 1);
  ASSERT_EQ(ASN1_TIME_check(offset_time.get()), 1);
  ASSERT_EQ(X509_set1_notBefore(cert.get(), offset_time.get()), 1);
  ASSERT_EQ(X509_set1_notAfter(cert.get(), offset_time.get()), 1);

  const ncrypto::BIOPointer valid_from = cert.view().getValidFrom();
  const ncrypto::BIOPointer valid_to = cert.view().getValidTo();
  ASSERT_TRUE(valid_from);
  ASSERT_TRUE(valid_to);
  EXPECT_EQ(BioContents(valid_from), "Bad time value");
  EXPECT_EQ(BioContents(valid_to), "Bad time value");
  EXPECT_FALSE(cert.view().getValidFromTime().has_value());
  EXPECT_FALSE(cert.view().getValidToTime().has_value());
}
#endif

TEST(NodeCrypto, PrintsEmptyX509Names) {
  ncrypto::X509Pointer cert(X509_new());
  X509NamePointer empty_name(X509_NAME_new());
  ASSERT_TRUE(cert);
  ASSERT_TRUE(empty_name);
  ASSERT_EQ(X509_set_subject_name(cert.get(), empty_name.get()), 1);
  ASSERT_EQ(X509_set_issuer_name(cert.get(), empty_name.get()), 1);

  const ncrypto::BIOPointer subject = cert.view().getSubject();
  const ncrypto::BIOPointer issuer = cert.view().getIssuer();
  ASSERT_TRUE(subject);
  ASSERT_TRUE(issuer);
  EXPECT_EQ(BioContents(subject), "");
  EXPECT_EQ(BioContents(issuer), "");
}

TEST(NodeCrypto, X509FormattingPreservesUnrelatedErrors) {
  ncrypto::X509Pointer cert(X509_new());
  X509NamePointer empty_name(X509_NAME_new());
  ASSERT_TRUE(cert);
  ASSERT_TRUE(empty_name);
  ASSERT_EQ(X509_set_subject_name(cert.get(), empty_name.get()), 1);

  ERR_clear_error();
#if OPENSSL_VERSION_MAJOR >= 3
  ERR_raise(ERR_LIB_USER, 1);
#else
  ERR_put_error(ERR_LIB_USER, 0, 1, __FILE__, __LINE__);
#endif
  const unsigned long expected = ERR_peek_error();  // NOLINT(runtime/int)
  ASSERT_NE(expected, 0UL);

  const ncrypto::BIOPointer subject = cert.view().getSubject();
  ASSERT_TRUE(subject);
  EXPECT_EQ(ERR_peek_error(), expected);
  ERR_clear_error();
}

TEST(NodeCrypto, RejectsEmptyX509SubjectAltNames) {
  GeneralNamesPointer names(sk_GENERAL_NAME_new_null());
  ASSERT_TRUE(names);

  X509ExtensionPointer san(
      X509V3_EXT_i2d(NID_subject_alt_name, 0, names.get()));
  ASSERT_TRUE(san);
  ncrypto::BIOPointer output = ncrypto::BIOPointer::NewMem();
  ASSERT_TRUE(output);
  EXPECT_FALSE(ncrypto::SafeX509SubjectAltNamePrint(output, san.get()));
  EXPECT_TRUE(BioContents(output).empty());
}

TEST(NodeCrypto, RejectsEmptyX509AuthorityInfoAccess) {
  AccessDescriptionsPointer descriptions(sk_ACCESS_DESCRIPTION_new_null());
  ASSERT_TRUE(descriptions);

  X509ExtensionPointer info_access(
      X509V3_EXT_i2d(NID_info_access, 0, descriptions.get()));
  ASSERT_TRUE(info_access);
  ncrypto::BIOPointer output = ncrypto::BIOPointer::NewMem();
  ASSERT_TRUE(output);
  EXPECT_FALSE(ncrypto::SafeX509InfoAccessPrint(output, info_access.get()));
  EXPECT_TRUE(BioContents(output).empty());
}

TEST(NodeCrypto, PrintsLongX509ExtensionObjectIdentifiers) {
  const std::string oid_text = LongObjectIdentifier();
  ASSERT_GT(oid_text.size(), 256U);

  GeneralNamesPointer names(sk_GENERAL_NAME_new_null());
  GeneralNamePointer name(GENERAL_NAME_new());
  ASN1ObjectPointer oid(OBJ_txt2obj(oid_text.c_str(), 1));
  ASSERT_TRUE(names);
  ASSERT_TRUE(name);
  ASSERT_TRUE(oid);
  GENERAL_NAME_set0_value(name.get(), GEN_RID, oid.release());
  ASSERT_GT(sk_GENERAL_NAME_push(names.get(), name.get()), 0);
  name.release();

  X509ExtensionPointer san(
      X509V3_EXT_i2d(NID_subject_alt_name, 0, names.get()));
  ASSERT_TRUE(san);
  ncrypto::BIOPointer output = ncrypto::BIOPointer::NewMem();
  ASSERT_TRUE(output);
  ASSERT_TRUE(ncrypto::SafeX509SubjectAltNamePrint(output, san.get()));
  EXPECT_EQ(BioContents(output), "Registered ID:" + oid_text);

  AccessDescriptionsPointer descriptions(sk_ACCESS_DESCRIPTION_new_null());
  AccessDescriptionPointer description(ACCESS_DESCRIPTION_new());
  ASN1ObjectPointer method(OBJ_txt2obj(oid_text.c_str(), 1));
  ASN1StringPointer uri(ASN1_IA5STRING_new());
  ASSERT_TRUE(descriptions);
  ASSERT_TRUE(description);
  ASSERT_TRUE(method);
  ASSERT_TRUE(uri);
  ASSERT_EQ(ASN1_STRING_set(uri.get(), "https://example.com/", -1), 1);
  ASN1_OBJECT_free(description->method);
  description->method = method.release();
  GENERAL_NAME_set0_value(description->location, GEN_URI, uri.release());
  ASSERT_GT(sk_ACCESS_DESCRIPTION_push(descriptions.get(), description.get()),
            0);
  description.release();

  X509ExtensionPointer info_access(
      X509V3_EXT_i2d(NID_info_access, 0, descriptions.get()));
  ASSERT_TRUE(info_access);
  output = ncrypto::BIOPointer::NewMem();
  ASSERT_TRUE(output);
  ASSERT_TRUE(ncrypto::SafeX509InfoAccessPrint(output, info_access.get()));
  std::string expected = oid_text + " - URI:https://example.com/";
#if defined(OPENSSL_IS_BORINGSSL) || OPENSSL_VERSION_MAJOR < 3
  expected += "\n";
#endif
  EXPECT_EQ(BioContents(output), expected);
}
