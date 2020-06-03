#include "kmsp11/util/crypto_utils.h"

#include <fstream>

#include "absl/strings/escaping.h"
#include "gmock/gmock.h"
#include "kmsp11/test/test_status_macros.h"
#include "openssl/bn.h"
#include "openssl/bytestring.h"
#include "openssl/ec_key.h"
#include "openssl/obj.h"
#include "tools/cpp/runfiles/runfiles.h"

namespace kmsp11 {
namespace {

using ::bazel::tools::cpp::runfiles::Runfiles;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;

StatusOr<std::string> LoadRunfile(absl::string_view filename) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(&error));
  if (!runfiles) {
    return absl::InternalError(
        absl::StrCat("could not load runfiles: ", error));
  }

  std::string location = runfiles->Rlocation(
      absl::StrCat("com_google_kmstools/kmsp11/util/testdata/", filename));
  std::ifstream runfile(location, std::ifstream::in | std::ifstream::binary);
  return std::string((std::istreambuf_iterator<char>(runfile)),
                     (std::istreambuf_iterator<char>()));
}

TEST(MarshalEcParametersTest, CurveOidMarshaled) {
  // Generate a new P-384 key
  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_secp384r1));
  EXPECT_TRUE(EC_KEY_generate_key(key.get()));

  // Serialize the EC parameters (group)
  ASSERT_OK_AND_ASSIGN(std::string oid_der, MarshalEcParametersDer(key.get()));

  // Deserialize the EC parameters and read the curve's OID
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(oid_der.data()),
           oid_der.size());
  CBS oid;
  EXPECT_TRUE(CBS_get_asn1(&cbs, &oid, CBS_ASN1_OBJECT));

  // Ensure the retrieved OID matches the expected
  EXPECT_EQ(OBJ_cbs2nid(&oid), OBJ_txt2nid("1.3.132.0.34"));
}

TEST(MarshalEcPointTest, PointMarshaled) {
  // Generate a new P-256 key
  bssl::UniquePtr<EC_GROUP> group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_KEY> key(EC_KEY_new());
  EXPECT_TRUE(EC_KEY_set_group(key.get(), group.get()));
  EXPECT_TRUE(EC_KEY_generate_key(key.get()));

  // Serialize the public key point
  ASSERT_OK_AND_ASSIGN(std::string point_der, MarshalEcPointDer(key.get()));

  // Deserialize the public key point
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(group.get()));
  bssl::UniquePtr<BN_CTX> bn_ctx(BN_CTX_new());
  EXPECT_TRUE(
      EC_POINT_oct2point(group.get(), point.get(),
                         reinterpret_cast<const uint8_t*>(point_der.data()),
                         point_der.size(), bn_ctx.get()));

  // Ensure the deserialized point matches the original
  EXPECT_EQ(EC_POINT_cmp(group.get(), EC_KEY_get0_public_key(key.get()),
                         point.get(), bn_ctx.get()),
            0);
}

TEST(ParseAndMarshalPublicKeyTest, EcKey) {
  // Parse the public key in PEM format.
  ASSERT_OK_AND_ASSIGN(std::string pem, LoadRunfile("ec_p256_public.pem"));
  ASSERT_OK_AND_ASSIGN(bssl::UniquePtr<EVP_PKEY> parsed_key,
                       ParseX509PublicKeyPem(pem));

  // Marshal the public key in DER format.
  ASSERT_OK_AND_ASSIGN(std::string got_der,
                       MarshalX509PublicKeyDer(parsed_key.get()));

  // Ensure the marshaled key matches the expected.
  ASSERT_OK_AND_ASSIGN(std::string want_der, LoadRunfile("ec_p256_public.der"));
  EXPECT_EQ(got_der, want_der);
}

TEST(ParseAndMarshalPublicKeyTest, RsaKey) {
  // Parse the public key in PEM format.
  ASSERT_OK_AND_ASSIGN(std::string pem, LoadRunfile("rsa_2048_public.pem"));
  ASSERT_OK_AND_ASSIGN(bssl::UniquePtr<EVP_PKEY> parsed_key,
                       ParseX509PublicKeyPem(pem));

  // Marshal the public key in DER format.
  ASSERT_OK_AND_ASSIGN(std::string got_der,
                       MarshalX509PublicKeyDer(parsed_key.get()));

  // Ensure the marshaled key matches the expected.
  ASSERT_OK_AND_ASSIGN(std::string want_der,
                       LoadRunfile("rsa_2048_public.der"));
  EXPECT_EQ(got_der, want_der);
}

TEST(SslErrorToStringTest, ErrorEmitted) {
  bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new_by_curve_name(0));
  EXPECT_THAT(ec_key, IsNull());
  EXPECT_THAT(SslErrorToString(), HasSubstr("UNKNOWN_GROUP"));
}

TEST(SslErrorToStringTest, EmptyStringOnNoError) {
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  EXPECT_THAT(ec_key, Not(IsNull()));
  EXPECT_THAT(SslErrorToString(), IsEmpty());
}

}  // namespace
}  // namespace kmsp11