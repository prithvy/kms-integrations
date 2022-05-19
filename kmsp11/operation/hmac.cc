// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "kmsp11/operation/hmac.h"

#include <string_view>

#include "kmsp11/openssl.h"
#include "kmsp11/operation/crypter_interfaces.h"
#include "kmsp11/operation/preconditions.h"
#include "kmsp11/util/crypto_utils.h"
#include "kmsp11/util/errors.h"
#include "kmsp11/util/status_macros.h"

namespace kmsp11 {
namespace {

constexpr size_t kMaxMacDataBytes = 64 * 1024;

absl::StatusOr<CK_KEY_TYPE> KeyTypeForMechanism(const CK_MECHANISM* mechanism) {
  switch (mechanism->mechanism) {
    case CKM_SHA_1_HMAC:
      return CKK_SHA_1_HMAC;
    case CKM_SHA224_HMAC:
      return CKK_SHA224_HMAC;
    case CKM_SHA256_HMAC:
      return CKK_SHA256_HMAC;
    case CKM_SHA512_HMAC:
      return CKK_SHA512_HMAC;
    default:
      return NewInternalError(
          absl::StrFormat("Cannot get CK_KEY_TYPE for mechanism %#x",
                          mechanism->mechanism),
          SOURCE_LOCATION);
  }
}

// A SignerInterface implementation that makes HMAC signatures using Cloud KMS.
class HmacSigner : public SignerInterface {
 public:
  static absl::StatusOr<std::unique_ptr<SignerInterface>> New(
      std::shared_ptr<Object> key, const CK_MECHANISM* mechanism,
      size_t signature_length);

  size_t signature_length() override { return signature_length_; };
  Object* object() override { return object_.get(); };

  absl::Status Sign(KmsClient* client, absl::Span<const uint8_t> data,
                    absl::Span<uint8_t> signature) override;
  absl::Status SignUpdate(KmsClient* client,
                          absl::Span<const uint8_t> data) override;
  absl::Status SignFinal(KmsClient* client,
                         absl::Span<uint8_t> signature) override;

  virtual ~HmacSigner() {}

 private:
  HmacSigner(std::shared_ptr<Object> object, size_t signature_length)
      : signature_length_(signature_length), object_(object) {}

  const size_t signature_length_;
  std::shared_ptr<Object> object_;
  std::optional<std::vector<uint8_t>> buffer_;
};

absl::StatusOr<std::unique_ptr<SignerInterface>> HmacSigner::New(
    std::shared_ptr<Object> key, const CK_MECHANISM* mechanism,
    size_t signature_length) {
  ASSIGN_OR_RETURN(CK_KEY_TYPE key_type, KeyTypeForMechanism(mechanism));
  RETURN_IF_ERROR(CheckKeyPreconditions(key_type, CKO_SECRET_KEY,
                                        mechanism->mechanism, key.get()));
  RETURN_IF_ERROR(EnsureNoParameters(mechanism));

  return std::unique_ptr<SignerInterface>(
      new HmacSigner(key, signature_length));
}

absl::Status HmacSigner::Sign(KmsClient* client, absl::Span<const uint8_t> data,
                              absl::Span<uint8_t> signature) {
  if (buffer_) {
    return FailedPreconditionError(
        "Sign cannot be used to terminate a multi-part signing operation",
        CKR_FUNCTION_FAILED, SOURCE_LOCATION);
  }

  if (data.size() > kMaxMacDataBytes) {
    return NewInvalidArgumentError(
        absl::StrFormat("data length (%d bytes) exceeds maximum allowed %d",
                        data.size(), kMaxMacDataBytes),
        CKR_DATA_LEN_RANGE, SOURCE_LOCATION);
  }

  if (signature.size() != signature_length()) {
    return NewInternalError(
        absl::StrFormat(
            "provided signature buffer has incorrect size (got %d, want %d)",
            signature.size(), signature_length()),
        SOURCE_LOCATION);
  }

  kms_v1::MacSignRequest req;
  req.set_name(std::string(object_->kms_key_name()));
  req.set_data(
      std::string(reinterpret_cast<const char*>(data.data()), data.size()));

  ASSIGN_OR_RETURN(kms_v1::MacSignResponse resp, client->MacSign(req));
  std::copy(resp.mac().begin(), resp.mac().end(), signature.begin());
  return absl::OkStatus();
}

absl::Status HmacSigner::SignUpdate(KmsClient* client,
                                            absl::Span<const uint8_t> data) {
  if (!buffer_) {
    buffer_.emplace(data.begin(), data.end());
    return absl::OkStatus();
  }

  if (data.size() + buffer_->size() > kMaxMacDataBytes) {
    return NewInvalidArgumentError(
        absl::StrFormat(
            "data length (%d bytes) exceeds maximum allowed (%d bytes)",
            data.size() + buffer_->size(), kMaxMacDataBytes),
        CKR_DATA_LEN_RANGE, SOURCE_LOCATION);
  }

  size_t old_size = buffer_->size();
  buffer_->resize(old_size + data.size());
  std::copy_n(data.begin(), data.size(), buffer_->begin() + old_size);

  return absl::OkStatus();
}

absl::Status HmacSigner::SignFinal(KmsClient* client,
                                   absl::Span<uint8_t> signature) {
  if (!buffer_) {
    return FailedPreconditionError(
        "SignUpdate needs to be called prior to terminating a multi-part "
        "signing operation",
        CKR_FUNCTION_FAILED, SOURCE_LOCATION);
  }

  if (signature.size() != signature_length()) {
    return NewInternalError(
        absl::StrFormat(
            "provided signature buffer has incorrect size (got %d, want %d)",
            signature.size(), signature_length()),
        SOURCE_LOCATION);
  }

  kms_v1::MacSignRequest req;
  req.set_name(std::string(object_->kms_key_name()));
  req.set_data(std::string(reinterpret_cast<const char*>(buffer_->data()),
                           buffer_->size()));

  ASSIGN_OR_RETURN(kms_v1::MacSignResponse resp, client->MacSign(req));
  std::copy(resp.mac().begin(), resp.mac().end(), signature.begin());
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<SignerInterface>> NewHmacSigner(
    std::shared_ptr<Object> key, const CK_MECHANISM* mechanism) {
  switch (mechanism->mechanism) {
    case CKM_SHA_1_HMAC:
      return HmacSigner::New(key, mechanism, 20);
    case CKM_SHA224_HMAC:
      return HmacSigner::New(key, mechanism, 28);
    case CKM_SHA256_HMAC:
      return HmacSigner::New(key, mechanism, 32);
    case CKM_SHA512_HMAC:
      return HmacSigner::New(key, mechanism, 64);
    default:
      return NewInternalError(
          absl::StrFormat("Mechanism %#x not supported for HMAC signing",
                          mechanism->mechanism),
          SOURCE_LOCATION);
  }
}

}  // namespace kmsp11