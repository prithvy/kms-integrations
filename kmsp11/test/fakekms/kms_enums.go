package fakekms

import (
	"crypto/elliptic"
	"fmt"

	kmspb "google.golang.org/genproto/googleapis/cloud/kms/v1"
)

// algDef contains details about a KMS algorithm
type algDef struct {
	Purpose    kmspb.CryptoKey_CryptoKeyPurpose
	KeyFactory keyFactory
}

func (a algDef) Asymmetric() bool {
	switch a.Purpose {
	case kmspb.CryptoKey_ASYMMETRIC_DECRYPT, kmspb.CryptoKey_ASYMMETRIC_SIGN:
		return true
	default:
		return false
	}
}

// algorithms maps KMS algorithms to their definitions
var algorithms = map[kmspb.CryptoKeyVersion_CryptoKeyVersionAlgorithm]algDef{
	// ENCRYPT_DECRYPT
	kmspb.CryptoKeyVersion_GOOGLE_SYMMETRIC_ENCRYPTION: {
		Purpose:    kmspb.CryptoKey_ENCRYPT_DECRYPT,
		KeyFactory: symmetricKeyFactory(256),
	},

	// ASYMMETRIC_DECRYPT
	kmspb.CryptoKeyVersion_RSA_DECRYPT_OAEP_2048_SHA256: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_DECRYPT,
		KeyFactory: rsaKeyFactory(2048),
	},
	kmspb.CryptoKeyVersion_RSA_DECRYPT_OAEP_3072_SHA256: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_DECRYPT,
		KeyFactory: rsaKeyFactory(3072),
	},
	kmspb.CryptoKeyVersion_RSA_DECRYPT_OAEP_4096_SHA256: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_DECRYPT,
		KeyFactory: rsaKeyFactory(4096),
	},
	kmspb.CryptoKeyVersion_RSA_DECRYPT_OAEP_4096_SHA512: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_DECRYPT,
		KeyFactory: rsaKeyFactory(4096),
	},

	// ASYMMETRIC_SIGN
	kmspb.CryptoKeyVersion_EC_SIGN_P256_SHA256: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_SIGN,
		KeyFactory: &ecKeyFactory{elliptic.P256()},
	},
	kmspb.CryptoKeyVersion_EC_SIGN_P384_SHA384: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_SIGN,
		KeyFactory: &ecKeyFactory{elliptic.P384()},
	},

	kmspb.CryptoKeyVersion_RSA_SIGN_PKCS1_2048_SHA256: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_SIGN,
		KeyFactory: rsaKeyFactory(2048),
	},
	kmspb.CryptoKeyVersion_RSA_SIGN_PKCS1_3072_SHA256: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_SIGN,
		KeyFactory: rsaKeyFactory(3072),
	},
	kmspb.CryptoKeyVersion_RSA_SIGN_PKCS1_4096_SHA256: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_SIGN,
		KeyFactory: rsaKeyFactory(4096),
	},
	kmspb.CryptoKeyVersion_RSA_SIGN_PKCS1_4096_SHA512: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_SIGN,
		KeyFactory: rsaKeyFactory(4096),
	},

	kmspb.CryptoKeyVersion_RSA_SIGN_PSS_2048_SHA256: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_SIGN,
		KeyFactory: rsaKeyFactory(2048),
	},
	kmspb.CryptoKeyVersion_RSA_SIGN_PSS_3072_SHA256: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_SIGN,
		KeyFactory: rsaKeyFactory(3072),
	},
	kmspb.CryptoKeyVersion_RSA_SIGN_PSS_4096_SHA256: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_SIGN,
		KeyFactory: rsaKeyFactory(4096),
	},
	kmspb.CryptoKeyVersion_RSA_SIGN_PSS_4096_SHA512: {
		Purpose:    kmspb.CryptoKey_ASYMMETRIC_SIGN,
		KeyFactory: rsaKeyFactory(4096),
	},
}

// nameForValue retrieves the mapped name corresponding to value, or a string representation
// of the numeric value if the value is unknown.
func nameForValue(names map[int32]string, value int32) string {
	if name, ok := names[value]; ok {
		return name
	}
	return fmt.Sprintf("%d", value)
}

// validateProtectionLevel returns nil on success, or Unimplemented if the supplied protection
// level is not supported. Note that PROTECTION_LEVEL_UNSPECIFIED is not a supported level.
func validateProtectionLevel(pl kmspb.ProtectionLevel) error {
	switch pl {
	case kmspb.ProtectionLevel_SOFTWARE, kmspb.ProtectionLevel_HSM:
		return nil
	default:
		return errUnimplemented("unsupported protection level: %s",
			nameForValue(kmspb.ProtectionLevel_name, int32(pl)))
	}
}

// algorithmDef returns an algDef corresponding to the supplied algorithm, or Unimplemented
// if the algorithm is not supported.
func algorithmDef(alg kmspb.CryptoKeyVersion_CryptoKeyVersionAlgorithm) (algDef, error) {
	def, ok := algorithms[alg]
	if !ok {
		return algDef{}, errUnimplemented("unsupported algorithm: %s",
			nameForValue(kmspb.CryptoKeyVersion_CryptoKeyVersionAlgorithm_name, int32(alg)))
	}
	return def, nil
}

// validateAlgorithm returns nil on success, Unimplemented if the supplied algorithm is not
// supported, or InvalidArgument if the algorithm is inconsistent with the supplied purpose.
// Note that CRYPTO_KEY_VERSION_ALGORITHM_UNSPECIFIED is not a supported algorithm.
func validateAlgorithm(alg kmspb.CryptoKeyVersion_CryptoKeyVersionAlgorithm, purpose kmspb.CryptoKey_CryptoKeyPurpose) error {
	def, err := algorithmDef(alg)
	if err != nil {
		return err
	}
	if purpose != def.Purpose {
		return errInvalidArgument("algorithm %s is not valid for purpose %s",
			nameForValue(kmspb.CryptoKeyVersion_CryptoKeyVersionAlgorithm_name, int32(alg)),
			nameForValue(kmspb.CryptoKey_CryptoKeyPurpose_name, int32(purpose)))
	}
	return nil
}