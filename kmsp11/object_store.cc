#include "kmsp11/object_store.h"

#include "kmsp11/util/crypto_utils.h"
#include "kmsp11/util/errors.h"
#include "kmsp11/util/status_macros.h"

namespace kmsp11 {
namespace {

// Cryptoki defines CK_OBJECT_HANDLE (and CK_ULONG) as simply `unsigned long`,
// whose size is platform-dependent. Ensure that proto's uint64 is large enough
// to hold such a value on any platform we're compiling on.
static_assert(
    sizeof(CK_OBJECT_HANDLE) <= sizeof(google::protobuf::uint64),
    "object handles must fit in proto uint64 for proto compatibility");

using ObjectStoreEntry = ObjectStoreMap::value_type;

absl::StatusOr<std::vector<ObjectStoreEntry>> ParseStoreEntries(
    const ObjectStoreState& state) {
  std::vector<ObjectStoreEntry> entries;
  for (const AsymmetricKey& item : state.asymmetric_keys()) {
    ASSIGN_OR_RETURN(bssl::UniquePtr<EVP_PKEY> public_key,
                     ParseX509PublicKeyDer(item.public_key_der()));
    ASSIGN_OR_RETURN(
        KeyPair keypair,
        Object::NewKeyPair(item.crypto_key_version(), public_key.get()));

    if (item.public_key_handle() == 0) {
      return absl::InvalidArgumentError("public_key_handle is unset");
    }
    entries.emplace_back(
        item.public_key_handle(),
        std::make_shared<Object>(std::move(keypair.public_key)));

    if (item.private_key_handle() == 0) {
      return absl::InvalidArgumentError("private_key_handle is unset");
    }
    entries.emplace_back(
        item.private_key_handle(),
        std::make_shared<Object>(std::move(keypair.private_key)));

    if (item.has_certificate()) {
      if (item.certificate().handle() == 0) {
        return absl::InvalidArgumentError("certificate_handle is unset");
      }
      ASSIGN_OR_RETURN(bssl::UniquePtr<X509> x509,
                       ParseX509CertificateDer(item.certificate().x509_der()));
      ASSIGN_OR_RETURN(Object cert, Object::NewCertificate(
                                        item.crypto_key_version(), x509.get()));
      entries.emplace_back(item.certificate().handle(),
                           std::make_shared<Object>(std::move(cert)));
    }
  }
  return entries;
}

}  // namespace

absl::StatusOr<ObjectStore> ObjectStore::New(const ObjectStoreState& state) {
  absl::StatusOr<std::vector<ObjectStoreEntry>> entries =
      ParseStoreEntries(state);
  if (!entries.ok()) {
    return NewInvalidArgumentError(
        absl::StrCat("failure building ObjectStore: ",
                     entries.status().message()),
        CKR_DEVICE_ERROR, SOURCE_LOCATION);
  }

  ObjectStore store(ObjectStoreMap(entries->begin(), entries->end()));
  if (store.entries_.size() != entries->size()) {
    return NewInvalidArgumentError(
        absl::StrFormat("duplicate handle detected: "
                        "store.entries_.size()=%d; entries.size()=%d",
                        store.entries_.size(), entries->size()),
        CKR_DEVICE_ERROR, SOURCE_LOCATION);
  }

  return store;
}

absl::StatusOr<std::shared_ptr<Object>> ObjectStore::GetObject(
    CK_OBJECT_HANDLE handle) const {
  ObjectStoreMap::const_iterator it = entries_.find(handle);
  if (it == entries_.end()) {
    return HandleNotFoundError(handle, CKR_OBJECT_HANDLE_INVALID,
                               SOURCE_LOCATION);
  }

  return it->second;
}

std::vector<CK_OBJECT_HANDLE> ObjectStore::Find(
    std::function<bool(const Object&)> predicate) const {
  std::vector<CK_OBJECT_HANDLE> handles;
  for (const ObjectStoreEntry& entry : entries_) {
    if (predicate(*entry.second)) {
      handles.push_back(entry.first);
    }
  }
  return handles;
}

}  // namespace kmsp11
