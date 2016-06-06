#ifndef _ALLJOYN_PERMISSION_CONFIGURATOR_H
#define _ALLJOYN_PERMISSION_CONFIGURATOR_H
/**
 * @file
 * This file defines the Permission Configurator class that exposes some permission
 * management capabilities to the application.
 */

/******************************************************************************
 * Copyright AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#ifndef __cplusplus
#error Only include PermissionConfigurator.h in C++ code.
#endif

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/CertificateECC.h>
#include <alljoyn/Status.h>
#include <alljoyn/PermissionPolicy.h>

namespace ajn {

/**
 * Class to allow the application to manage some limited permission feature.
 */

class PermissionConfigurator {

  public:

    typedef enum {
        NOT_CLAIMABLE = 0, ///< The application is not claimed and not accepting claim requests.
        CLAIMABLE = 1,     ///< The application is not claimed and is accepting claim requests.
        CLAIMED = 2,       ///< The application is claimed and can be configured.
        NEED_UPDATE = 3    ///< The application is claimed, but requires a configuration update (after a software upgrade).
    } ApplicationState;

    /**
     * @brief returns the string representation of the application state.
     *
     * @param[in] as    application state.
     *
     * @return string   representation of the application state.
     */
    static const char* ToString(const PermissionConfigurator::ApplicationState as) {
        switch (as) {
        case PermissionConfigurator::NOT_CLAIMABLE:
            return "NOT CLAIMABLE";

        case PermissionConfigurator::CLAIMABLE:
            return "CLAIMABLE";

        case PermissionConfigurator::CLAIMED:
            return "CLAIMED";

        case PermissionConfigurator::NEED_UPDATE:
            return "NEED UPDATE";
        }

        return "UNKNOWN";
    }


    /**@name ClaimCapabilities */
    // {@
    typedef uint16_t ClaimCapabilities;
    typedef enum {
        CAPABLE_ECDHE_NULL = 0x01,
        CAPABLE_ECDHE_PSK = 0x02,   ///< Deprecated, will be removed in a future release.
        CAPABLE_ECDHE_ECDSA = 0x04,
        CAPABLE_ECDHE_SPEKE = 0x08
    } ClaimCapabilityMasks;
    // @}

    /** Default ClaimCapabilities: NULL, PSK and SPEKE. */
    static const uint16_t CLAIM_CAPABILITIES_DEFAULT;

    /**@name ClaimCapabilityAdditionalInfo */
    // {@
    typedef uint16_t ClaimCapabilityAdditionalInfo;
    typedef enum {
        PSK_GENERATED_BY_SECURITY_MANAGER = 0x01,   ///< The pre-shared key or password is generated by the security manager
        PSK_GENERATED_BY_APPLICATION      = 0x02    ///< The pre-shared key or password is generated by the application
    } ClaimCapabilityAdditionalInfoMasks;
    // @}
    /**
     * Constructor
     *
     */
    PermissionConfigurator(BusAttachment& bus);

    /**
     * virtual destructor
     */
    virtual ~PermissionConfigurator();

    /**
     * Set the permission manifest for the application.
     * @param rules the permission rules.
     * @param count the number of permission rules
     * @return ER_OK if successful; otherwise, an error code.
     */
    QStatus SetPermissionManifestTemplate(PermissionPolicy::Rule* rules, size_t count);

    /**
     * Set the manifest template for the application from an XML.
     *
     * @params manifestTemplateXml XML containing the manifest template.
     *
     * @return ER_OK if successful; otherwise, an error code.
     */
    QStatus SetManifestTemplateFromXml(AJ_PCSTR manifestTemplateXml);

    /**
     * Retrieve the state of the application.
     * @param[out] applicationState the application state
     * @return
     *      - #ER_OK if successful
     *      - #ER_NOT_IMPLEMENTED if the method is not implemented
     *      - #ER_FEATURE_NOT_AVAILABLE if the value is not known
     */
    QStatus GetApplicationState(ApplicationState& applicationState) const;

    /**
     * Set the application state.  The state can't be changed from CLAIMED to
     * CLAIMABLE.
     * @param newState The new application state
     * @return
     *      - #ER_OK if action is allowed.
     *      - #ER_INVALID_APPLICATION_STATE if the state can't be changed
     *      - #ER_NOT_IMPLEMENTED if the method is not implemented
     */
    QStatus SetApplicationState(ApplicationState newState);

    /**
     * Retrieve the public key info for the signing key.
     * @param[out] keyInfo the public key info
     * @return ER_OK if successful; otherwise, an error code.
     */
    QStatus GetSigningPublicKey(qcc::KeyInfoECC& keyInfo);

    /**
     * Sign the X509 certificate using the signing key
     * @param[out] cert the certificate to be signed
     * @return ER_OK if successful; otherwise, an error code.
     */
    QStatus SignCertificate(qcc::CertificateX509& cert);

    /**
     * Sign a manifest using the signing key, and bind the manifest to a particular identity
     * certificate by providing its thumbprint. For this manifest to be valid when later used,
     * the signing key of this PermissionConfigurator must be the signing key that issued the
     * certificate. Callers must ensure the correct key is used.
     *
     * @param[in] subjectThumbprint Identity certificate thumbprint to use the manifest
     * @param[in,out] manifest Manifest to sign
     * @return ER_OK of successful; otherwise, an error code.
     */
    QStatus SignManifest(const std::vector<uint8_t>& subjectThumbprint, Manifest& manifest);

    /**
     * Sign a manifest using the signing key, and bind the manifest to a particular identity
     * certificate by providing the certificate. For this manifest to be valid when later used,
     * the signing key of this PermissionConfigurator must be the signing key that issued the
     * certificate. Callers must ensure the correct key is used; this method does not verify
     * the signing key was used to issue the provided certificate.
     *
     * @param[in] subjectCertificate Certificate to use the manifest. Certificate must already be
     *                               signed in order to encode its identity correctly in the manifest.
     * @param[in,out] manifest       Manifest to sign
     * @return ER_OK if successful; otherwise, an error code.
     */
    QStatus ComputeThumbprintAndSignManifest(const qcc::CertificateX509& subjectCertificate, Manifest& manifest);

    /**
     * Reset the permission settings by removing the manifest all the
     * trust anchors, installed policy and certificates. This call
     * must be invoked after the bus attachment has enable peer security.
     * @return ER_OK if successful; otherwise, an error code.
     * @see BusAttachment::EnablePeerSecurity
     */
    QStatus Reset();

    /**
     * Get the connected peer ECC public key if the connection uses the
     * ECDHE_ECDSA key exchange.
     * @param guid the peer guid
     * @param[out] publicKey the buffer to hold the ECC public key.
     * @return ER_OK if successful; otherwise, error code.
     */
    QStatus GetConnectedPeerPublicKey(const qcc::GUID128& guid, qcc::ECCPublicKey* publicKey);

    /**
     * Set the authentication mechanisms the application supports for the
     * claim process.  It is a bit mask.
     *
     * | Mask                | Description              |
     * |---------------------|--------------------------|
     * | CAPABLE_ECDHE_NULL  | claiming via ECDHE_NULL  |
     * | CAPABLE_ECDHE_PSK   | claiming via ECDHE_PSK   |
     * | CAPABLE_ECDHE_ECDSA | claiming via ECDHE_ECDSA |
     *
     * @param[in] claimCapabilities The authentication mechanisms the application supports
     *
     * @return
     *  - #ER_OK if successful
     *  - an error status indicating failure
     */
    QStatus SetClaimCapabilities(PermissionConfigurator::ClaimCapabilities claimCapabilities);

    /**
     * Get the authentication mechanisms the application supports for the
     * claim process.
     *
     * @param[out] claimCapabilities The authentication mechanisms the application supports
     *
     * @return
     *  - #ER_OK if successful
     *  - an error status indicating failure
     */
    QStatus GetClaimCapabilities(PermissionConfigurator::ClaimCapabilities& claimCapabilities) const;

    /**
     * Set the additional information on the claim capabilities.
     * It is a bit mask.
     *
     * | Mask                              | Description                       |
     * |-----------------------------------|-----------------------------------|
     * | PSK_GENERATED_BY_SECURITY_MANAGER | PSK generated by Security Manager |
     * | PSK_GENERATED_BY_APPLICATION      | PSK generated by application      |
     *
     * @param[in] additionalInfo The additional info
     *
     * @return
     *  - #ER_OK if successful
     *  - an error status indicating failure
     */
    QStatus SetClaimCapabilityAdditionalInfo(PermissionConfigurator::ClaimCapabilityAdditionalInfo additionalInfo);

    /**
     * Get the additional information on the claim capabilities.
     * @param[out] additionalInfo The additional info
     *
     * @return
     *  - #ER_OK if successful
     *  - an error status indicating failure
     */
    QStatus GetClaimCapabilityAdditionalInfo(PermissionConfigurator::ClaimCapabilityAdditionalInfo& additionalInfo) const;

  private:
    /**
     * @internal
     * Class for internal state of a PermissionConfigurator object.
     */
    class Internal;

    /**
     * @internal
     * Contains internal state of a PermissionConfigurator object.
     */
    PermissionConfigurator::Internal* m_internal;

    /**
     * Assignment operator is private.
     */
    PermissionConfigurator& operator=(const PermissionConfigurator& other);

    /**
     * Copy constructor is private.
     */
    PermissionConfigurator(const PermissionConfigurator& other);
};

}
#endif
