//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <APPX/Sign.h>
#include <APPX/Sink.h>
#include <cassert>
#include <cstdint>
#include <openssl/asn1t.h>
#include <libp11.h>
#include <vector>

namespace facebook {
namespace appx {
    namespace {
        namespace oid {
            // https://support.microsoft.com/en-us/kb/287547
            const char kSPCIndirectData[] = "1.3.6.1.4.1.311.2.1.4";
            const char kSPCSipinfo[] = "1.3.6.1.4.1.311.2.1.30";
            const char kSPCSpOpusInfo[] = "1.3.6.1.4.1.311.2.1.12";
            const char kSPCStatementType[] = "1.3.6.1.4.1.311.2.1.11";

            void Register()
            {
                OBJ_create_and_add_object(kSPCIndirectData, nullptr, nullptr);
                OBJ_create_and_add_object(kSPCSipinfo, nullptr, nullptr);
                OBJ_create_and_add_object(kSPCSpOpusInfo, nullptr, nullptr);
                OBJ_create_and_add_object(kSPCStatementType, nullptr, nullptr);
            }
        };

        namespace asn1 {
            // https://msdn.microsoft.com/en-us/gg463180.aspx

            struct SPCStatementType
            {
                ASN1_OBJECT *type;
            };
            DECLARE_ASN1_FUNCTIONS(SPCStatementType)
            using SPCStatementTypePtr =
                OpenSSLPtr<SPCStatementType, SPCStatementType_free>;

            struct SPCSpOpusInfo
            {
                ASN1_TYPE *programName;  // TODO(strager): SPCString.
                ASN1_TYPE *moreInfo;     // TODO(strager): SPCLink.
            };
            DECLARE_ASN1_FUNCTIONS(SPCSpOpusInfo)
            using SPCSpOpusInfoPtr =
                OpenSSLPtr<SPCSpOpusInfo, SPCSpOpusInfo_free>;

            struct DigestInfo
            {
                X509_ALGOR *digestAlgorithm;
                ASN1_OCTET_STRING *digest;
            };
            DECLARE_ASN1_FUNCTIONS(DigestInfo)

            struct SPCAttributeTypeAndOptionalValue
            {
                ASN1_OBJECT *type;
                ASN1_TYPE *value;  // SPCInfoValue
            };
            DECLARE_ASN1_FUNCTIONS(SPCAttributeTypeAndOptionalValue)

            // Undocumented.
            struct SPCInfoValue
            {
                ASN1_INTEGER *i1;
                ASN1_OCTET_STRING *s1;
                ASN1_INTEGER *i2;
                ASN1_INTEGER *i3;
                ASN1_INTEGER *i4;
                ASN1_INTEGER *i5;
                ASN1_INTEGER *i6;
            };
            DECLARE_ASN1_FUNCTIONS(SPCInfoValue)
            using SPCInfoValuePtr = OpenSSLPtr<SPCInfoValue, SPCInfoValue_free>;

            struct SPCIndirectDataContent
            {
                SPCAttributeTypeAndOptionalValue *data;
                DigestInfo *messageDigest;
            };
            DECLARE_ASN1_FUNCTIONS(SPCIndirectDataContent)
            using SPCIndirectDataContentPtr =
                OpenSSLPtr<SPCIndirectDataContent, SPCIndirectDataContent_free>;

            // clang-format off
            IMPLEMENT_ASN1_FUNCTIONS(SPCIndirectDataContent)
            ASN1_SEQUENCE(SPCIndirectDataContent) = {
                ASN1_SIMPLE(SPCIndirectDataContent, data,
                            SPCAttributeTypeAndOptionalValue),
                ASN1_SIMPLE(SPCIndirectDataContent, messageDigest, DigestInfo),
            } ASN1_SEQUENCE_END(SPCIndirectDataContent)

            IMPLEMENT_ASN1_FUNCTIONS(SPCAttributeTypeAndOptionalValue)
            ASN1_SEQUENCE(SPCAttributeTypeAndOptionalValue) = {
                ASN1_SIMPLE(SPCAttributeTypeAndOptionalValue, type,
                            ASN1_OBJECT),
                ASN1_OPT(SPCAttributeTypeAndOptionalValue, value, ASN1_ANY),
            } ASN1_SEQUENCE_END(SPCAttributeTypeAndOptionalValue)

            IMPLEMENT_ASN1_FUNCTIONS(SPCInfoValue)
            ASN1_SEQUENCE(SPCInfoValue) = {
                ASN1_SIMPLE(SPCInfoValue, i1, ASN1_INTEGER),
                ASN1_SIMPLE(SPCInfoValue, s1, ASN1_OCTET_STRING),
                ASN1_SIMPLE(SPCInfoValue, i2, ASN1_INTEGER),
                ASN1_SIMPLE(SPCInfoValue, i3, ASN1_INTEGER),
                ASN1_SIMPLE(SPCInfoValue, i4, ASN1_INTEGER),
                ASN1_SIMPLE(SPCInfoValue, i5, ASN1_INTEGER),
                ASN1_SIMPLE(SPCInfoValue, i6, ASN1_INTEGER),
            } ASN1_SEQUENCE_END(SPCInfoValue)

            IMPLEMENT_ASN1_FUNCTIONS(DigestInfo)
            ASN1_SEQUENCE(DigestInfo) = {
                ASN1_SIMPLE(DigestInfo, digestAlgorithm, X509_ALGOR),
                ASN1_SIMPLE(DigestInfo, digest, ASN1_OCTET_STRING),
            } ASN1_SEQUENCE_END(DigestInfo)

            ASN1_SEQUENCE(SPCSpOpusInfo) = {
                ASN1_OPT(SPCSpOpusInfo, programName, ASN1_ANY),
                ASN1_OPT(SPCSpOpusInfo, moreInfo, ASN1_ANY),
            } ASN1_SEQUENCE_END(SPCSpOpusInfo)
            IMPLEMENT_ASN1_FUNCTIONS(SPCSpOpusInfo)

            ASN1_SEQUENCE(SPCStatementType) = {
                ASN1_SIMPLE(SPCStatementType, type, ASN1_OBJECT),
            } ASN1_SEQUENCE_END(SPCStatementType)
            IMPLEMENT_ASN1_FUNCTIONS(SPCStatementType)
            // clang-format on
        }

        class EncodedASN1
        {
        public:
            template <typename T, int (*TEncode)(T *, std::uint8_t **)>
            static EncodedASN1 FromItem(T *item)
            {
                std::uint8_t *dataRaw = nullptr;
                int size = TEncode(item, &dataRaw);
                std::unique_ptr<std::uint8_t, Deleter> data(dataRaw);
                if (size < 0) {
                    throw OpenSSLException();
                }
                return EncodedASN1(std::move(data), size);
            }

            const std::uint8_t *Data() const
            {
                return this->data.get();
            }

            size_t Size() const
            {
                return this->size;
            }

            // Assumes the encoded ASN.1 represents a SEQUENCE and puts it into
            // an ASN1_STRING.
            //
            // The returned object holds a copy of this object's data.
            ASN1_STRINGPtr ToSequenceString()
            {
                ASN1_STRINGPtr string(ASN1_STRING_new());
                if (!string) {
                    throw OpenSSLException();
                }
                if (!ASN1_STRING_set(string.get(), this->Data(),
                                     this->Size())) {
                    throw OpenSSLException();
                }
                return string;
            }

            // Assumes the encoded ASN.1 represents a SEQUENCE and puts it into
            // an ASN1_TYPE.
            //
            // The returned object holds a copy of this object's data.
            ASN1_TYPEPtr ToSequenceType()
            {
                ASN1_STRINGPtr string = this->ToSequenceString();
                ASN1_TYPEPtr type(ASN1_TYPE_new());
                if (!type) {
                    throw OpenSSLException();
                }
                type->type = V_ASN1_SEQUENCE;
                type->value.sequence = string.release();
                return type;
            }

        private:
            struct Deleter
            {
                void operator()(std::uint8_t *data)
                {
                    if (data) {
                        OPENSSL_free(data);
                    }
                }
            };

            EncodedASN1(std::unique_ptr<std::uint8_t, Deleter> &&data,
                        size_t size)
                : data(std::move(data)), size(size)
            {
            }

            std::unique_ptr<std::uint8_t, Deleter> data;
            size_t size;
        };

        void MakeSPCInfoValue(asn1::SPCInfoValue &info)
        {
            // I have no idea what these numbers mean.
            static std::uint8_t s1Magic[] = {
                0x4B, 0xDF, 0xC5, 0x0A, 0x07, 0xCE, 0xE2, 0x4D,
                0xB7, 0x6E, 0x23, 0xC8, 0x39, 0xA0, 0x9F, 0xD1,
            };
            ASN1_INTEGER_set(info.i1, 0x01010000);
            ASN1_OCTET_STRING_set(info.s1, s1Magic, sizeof(s1Magic));
            ASN1_INTEGER_set(info.i2, 0x00000000);
            ASN1_INTEGER_set(info.i3, 0x00000000);
            ASN1_INTEGER_set(info.i4, 0x00000000);
            ASN1_INTEGER_set(info.i5, 0x00000000);
            ASN1_INTEGER_set(info.i6, 0x00000000);
        }

        void MakeIndirectDataContent(asn1::SPCIndirectDataContent &idc,
                                     const APPXDigests &digests)
        {
            using namespace asn1;

            ASN1_TYPEPtr algorithmParameter(ASN1_TYPE_new());
            if (!algorithmParameter) {
                throw OpenSSLException();
            }
            algorithmParameter->type = V_ASN1_NULL;

            SPCInfoValuePtr infoValue(SPCInfoValue_new());
            if (!infoValue) {
                throw OpenSSLException();
            }
            MakeSPCInfoValue(*infoValue);

            ASN1_TYPEPtr value =
                EncodedASN1::FromItem<asn1::SPCInfoValue,
                                      asn1::i2d_SPCInfoValue>(infoValue.get())
                    .ToSequenceType();

            {
                std::vector<std::uint8_t> digest;
                VectorSink sink(digest);
                digests.Write(sink);
                if (!ASN1_OCTET_STRING_set(idc.messageDigest->digest,
                                           digest.data(), digest.size())) {
                    throw OpenSSLException();
                }
            }

            idc.data->type = OBJ_txt2obj(oid::kSPCSipinfo, 1);
            idc.data->value = value.release();
            idc.messageDigest->digestAlgorithm->algorithm =
                OBJ_nid2obj(NID_sha256);
            idc.messageDigest->digestAlgorithm->parameter =
                algorithmParameter.release();
        }

        void AddAttributes(PKCS7_SIGNER_INFO *signerInfo)
        {
            // Add opus attribute.
            asn1::SPCSpOpusInfoPtr opus(asn1::SPCSpOpusInfo_new());
            if (!opus) {
                throw OpenSSLException();
            }
            ASN1_STRINGPtr opusValue =
                EncodedASN1::FromItem<asn1::SPCSpOpusInfo,
                                      asn1::i2d_SPCSpOpusInfo>(opus.get())
                    .ToSequenceString();
            if (!PKCS7_add_signed_attribute(signerInfo,
                                            OBJ_txt2nid(oid::kSPCSpOpusInfo),
                                            V_ASN1_SEQUENCE, opusValue.get())) {
                throw OpenSSLException();
            }
            opusValue.release();

            // Add content type attribute.
            if (!PKCS7_add_signed_attribute(
                    signerInfo, NID_pkcs9_contentType, V_ASN1_OBJECT,
                    OBJ_txt2obj(oid::kSPCIndirectData, 1))) {
                throw OpenSSLException();
            }

            // Add statement type attribute.
            asn1::SPCStatementTypePtr statementType(
                asn1::SPCStatementType_new());
            if (!statementType) {
                throw OpenSSLException();
            }
            statementType->type = OBJ_nid2obj(NID_ms_code_ind);
            ASN1_STRINGPtr statementTypeValue =
                EncodedASN1::FromItem<asn1::SPCStatementType,
                                      asn1::i2d_SPCStatementType>(
                    statementType.get())
                    .ToSequenceString();
            if (!PKCS7_add_signed_attribute(
                    signerInfo, OBJ_txt2nid(oid::kSPCStatementType),
                    V_ASN1_SEQUENCE, statementTypeValue.get())) {
                throw OpenSSLException();
            }
            statementTypeValue.release();
        }
    }

    namespace {
        struct CertificateFile
        {
            OpenSSLPtr<EVP_PKEY, EVP_PKEY_free> privateKey;
            OpenSSLPtr<X509, X509_free> certificate;
        };

        CertificateFile ReadCertificateFile(const std::string &path)
        {
            BIOPtr file(BIO_new_file(path.c_str(), "rb"));
            if (!file) {
                throw OpenSSLException(path);
            }
            PKCS12Ptr data(d2i_PKCS12_bio(file.get(), nullptr));
            if (!data) {
                throw OpenSSLException(path);
            }
            OpenSSLPtr<EVP_PKEY, EVP_PKEY_free> privateKey;
            OpenSSLPtr<X509, X509_free> certificate;
            {
                EVP_PKEY *privateKeyRaw;
                X509 *certificateRaw;
                if (!PKCS12_parse(data.get(), "", &privateKeyRaw,
                                  &certificateRaw, nullptr)) {
                    throw OpenSSLException(path);
                }
                privateKey.reset(privateKeyRaw);
                certificate.reset(certificateRaw);
            }
            if (!privateKey) {
                throw OpenSSLException();
            }
            if (!certificate) {
                throw OpenSSLException();
            }
            return CertificateFile{std::move(privateKey),
                                   std::move(certificate)};
        }

        OpenSSLPtr<PKCS7, PKCS7_free> Sign(CertificateFile certFile,
                                           const APPXDigests &digests)
        {
            // Create the signature.
            OpenSSLPtr<PKCS7, PKCS7_free> signature(PKCS7_new());
            if (!signature) {
                throw OpenSSLException();
            }
            if (!PKCS7_set_type(signature.get(), NID_pkcs7_signed)) {
                throw OpenSSLException();
            }
            PKCS7_SIGNER_INFO *signerInfo =
                PKCS7_add_signature(signature.get(), certFile.certificate.get(),
                                    certFile.privateKey.get(), EVP_sha256());
            if (!signerInfo) {
                throw OpenSSLException();
            }
            AddAttributes(signerInfo);

            if (!PKCS7_content_new(signature.get(), NID_pkcs7_data)) {
                throw OpenSSLException();
            }
            if (!PKCS7_add_certificate(signature.get(),
                                       certFile.certificate.get())) {
                throw OpenSSLException();
            }

            asn1::SPCIndirectDataContentPtr idc(asn1::SPCIndirectDataContent_new());
            MakeIndirectDataContent(*idc, digests);
            EncodedASN1 idcEncoded =
                EncodedASN1::FromItem<asn1::SPCIndirectDataContent,
                                      asn1::i2d_SPCIndirectDataContent>(idc.get());

            // TODO(strager): Use lower-level APIs to avoid OpenSSL injecting the
            // signingTime attribute.
            BIOPtr signedData(PKCS7_dataInit(signature.get(), NULL));
            if (!signedData) {
                throw OpenSSLException();
            }
            // Per RFC 2315 section 9.3:
            // "Only the contents octets of the DER encoding of that field are
            // digested, not the identifier octets or the length octets."
            // Strip off the length.
            if (idcEncoded.Size() < 2) {
                throw std::runtime_error("NYI");
            }
            if ((idcEncoded.Data()[1] & 0x80) == 0x00) {
                throw std::runtime_error("NYI");
            }
            std::size_t skip = 4;
            if (BIO_write(signedData.get(), idcEncoded.Data() + skip,
                          idcEncoded.Size() - skip) != idcEncoded.Size() - skip) {
                throw OpenSSLException();
            }
            if (BIO_flush(signedData.get()) != 1) {
                throw OpenSSLException();
            }
            if (!PKCS7_dataFinal(signature.get(), signedData.get())) {
                throw OpenSSLException();
            }

            // Set the content to an SpcIndirectDataContent. Must be done after
            // digesting the signed data.
            OpenSSLPtr<PKCS7, PKCS7_free> content(PKCS7_new());
            if (!content) {
                throw OpenSSLException();
            }
            content->type = OBJ_txt2obj(oid::kSPCIndirectData, 1);
            ASN1_TYPEPtr idcSequence = idcEncoded.ToSequenceType();
            content->d.other = idcSequence.get();
            if (!PKCS7_set_content(signature.get(), content.get())) {
                throw OpenSSLException();
            }
            content.release();
            idcSequence.release();

            return signature;
        }
    }

    OpenSSLPtr<PKCS7, PKCS7_free> SignFromCertFile(const std::string &certPath,
                                                   const APPXDigests &digests)
    {
        OpenSSL_add_all_algorithms();
        oid::Register();

        CertificateFile certFile = ReadCertificateFile(certPath);
        return Sign(std::move(certFile), digests);
    }

    OpenSSLPtr<PKCS7, PKCS7_free> SignFromSmartCard(const std::string& modulePath,
                                                    uint32_t slotId,
                                                    uint8_t keyId,
                                                    const std::string &pivPin,
                                                    const APPXDigests &digests)
    {
        OpenSSL_add_all_algorithms();
        oid::Register();
        OpenSSLPtr<PKCS11_CTX, PKCS11_CTX_free> ctx{ PKCS11_CTX_new() };
        if (!ctx) {
            throw OpenSSLException();
        }

        PKCS11_CTX_load( ctx.get(), modulePath.c_str() );
        OpenSSLPtr<PKCS11_CTX, PKCS11_CTX_unload> loadedCtx{ ctx.get() };

        PKCS11_SLOT* slots;
        uint32_t nbSlots;

        if (PKCS11_enumerate_slots( ctx.get(), &slots, &nbSlots )) {
            throw OpenSSLException();
        }
        std::unique_ptr<PKCS11_SLOT, std::function<void(PKCS11_SLOT*)>>
            slotsReleaser{slots, [nbSlots, c = ctx.get()](PKCS11_SLOT* slots) {
                    PKCS11_release_all_slots(c, slots, nbSlots);
            }};

        PKCS11_SLOT* s = nullptr;
        while ((s = PKCS11_find_next_token(ctx.get(), slots, nbSlots, s))) {
            PKCS11_KEY* keys;
            uint32_t nbKeys;
            if (PKCS11_get_slotid_from_slot(s) != slotId)
                continue;

            PKCS11_CERT* certs;
            uint32_t nbCerts;

            if (PKCS11_enumerate_certs( s->token, &certs, &nbCerts))
                continue;
            auto cert = &certs[0];

            // Check that there is a key that matches our request before loging in
            if (PKCS11_enumerate_public_keys(s->token, &keys, &nbKeys))
                throw OpenSSLException();
            PKCS11_KEY* k = nullptr;
            for (auto i = 0u; i < nbKeys; ++i) {
                k = &keys[i];
                // If there is an id and it matches, this is the correct slot/id
                // pair we're looking for. Now we'll need to fetch the private key
                if (k->id && k->id[0] == keyId)
                    break;
            }
            if (!k)
                continue;

            PKCS11_login( s, 0, pivPin.c_str() );

            OpenSSLPtr<EVP_PKEY, EVP_PKEY_free> privateKey{
                PKCS11_get_private_key(k)
            };
            if (!privateKey)
                continue;
            OpenSSLPtr<X509, X509_free> certificate{
                X509_dup(cert->x509)
            };
            if (!X509_check_private_key(certificate.get(), privateKey.get()))
                continue;

            CertificateFile certFile{std::move(privateKey),
                                     std::move(certificate)};
            return Sign(std::move(certFile), digests);
        }
        fprintf(stderr, "No usable key was found with slot %u and key id %u\n",
                slotId, keyId);
        return nullptr;
    }
}
}
