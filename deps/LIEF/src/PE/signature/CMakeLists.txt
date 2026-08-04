target_sources(LIB_LIEF PRIVATE
  Attribute.cpp
  ContentInfo.cpp
  GenericContent.cpp
  OIDToString.cpp
  PKCS9TSTInfo.cpp
  RsaInfo.cpp
  Signature.cpp
  SignatureParser.cpp
  SignerInfo.cpp
  SpcIndirectData.cpp
  x509.cpp
)

add_subdirectory(attributes)
