/******************************************************************************
 * Copyright (c) 2014, AllSeen Alliance. All rights reserved.
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

#ifndef SECURITYMANAGER_H_
#define SECURITYMANAGER_H_

#include <IdentityData.h>
#include <alljoyn/Status.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/PermissionPolicy.h>
#include <ApplicationInfo.h>
#include <AppGuildInfo.h>
#include <ApplicationListener.h>
#include <StorageConfig.h>
#include <Identity.h>
#include <Storage.h>
#include <memory>
#include <SecurityManagerConfig.h>

#include <qcc/String.h>
#include <qcc/X509Certificate.h>
#include <qcc/CryptoECC.h>

#include <qcc/Debug.h>
#define QCC_MODULE "SEC_MGR"

namespace ajn {
namespace securitymgr {
class SecurityManagerImpl;

/**
 * \class SecurityManager
 * \brief The SecurityManager enables the claiming of applications in a secure manner besides
 *        providing the needed affiliated functionalities.
 *
 * Internally it uses an ApplicationMonitor to track active applications.
 * A particular user has a SecurityManager object for each RoT he owns
 * In other words: 1 RoT = 1 SecurityManager
 */

/**
 * Callback function to ask the administrator to accept the manifest.
 */
typedef bool (*AcceptManifestCB)(const ApplicationInfo& appInfo,
                                 const PermissionPolicy::Rule* manifestRules,
                                 const size_t manifestRulesCount,
                                 void* cookie);

class SecurityManager {
  private:
    /* This constructor can only be called by the factory */
    /* Passing the private key in memory like this is not a good idea but that is the AllJoyn way*/

    SecurityManager(IdentityData* id,                                              // Supports NULL identity
                    ajn::BusAttachment* ba,
                    const qcc::ECCPublicKey& pubKey,
                    const qcc::ECCPrivateKey& privKey,
                    const StorageConfig& storageCfg,
                    const SecurityManagerConfig& smCfg);

    QStatus GetStatus() const;

    SecurityManager(const SecurityManager&);
    void operator=(const SecurityManager&);

    friend class SecurityManagerFactory;

  public:
    /**
     * \brief Claim an application if it was indeed claimable.
     *        This entails installing a RoT, generating an identity certificate (based on Aboutdata)
     *        and installing that certificate.
     *
     *  QUESTION:
     *   -What information do we cache locally: it probably makes sense to store which application we claimed ?
     *      Or can we retrieve this information at run-time (privacy concerns !)
     *      OTOH: if our RoT on the application gets removed by a second RoT, can we still guarantee cache consistency..
     *   -Is this a blocking call ? if the network would go down this method call can hang for quite some time.
     *      I guess in the PoC we could keep this blocking and later have an async call with a completion callback .
     *   -Probably QStatus is not good enough to convey all the information when things go wrong security-wise. Either extend QStatus or come up with a new error.
     *
     * \param[in] app the application info of the application that we need to add the RoT to
     * \param[in] idInfo the identity info of the identity we want to link the application
     * \param[in] amcb the callback function for accepting the manifest of the app
     * \param[in] cookie a pointer to application specific data. This pointer will be passed to class made to AcceptManifestCB.
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus ClaimApplication(const ApplicationInfo& app,
                             const IdentityInfo& idInfo,
                             AcceptManifestCB amcb,
                             void* cookie = NULL);

    /**
     * \brief Retrieve the manifest of the remote application.
     *
     * \param[in] appInfo the application from which the manifest should be retrieved.
     * \param[out] manifestRules an array of rules in the manifest of the application.
     * \param[out] manifestRulesCount the number of rules in the manifestRules array.
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus GetManifest(const ApplicationInfo& appInfo,
                        PermissionPolicy::Rule** manifestRules,
                        size_t* manifestRulesCount);

    /**
     * \brief Claim an application if it was indeed claimable.
     *        This entails installing a RoT, generating an identity certificate (based on the identity info)
     *        and installing that certificate.
     *
     * \param[in] app the application info of the application that we need to add the RoT to
     * \param[in] identityInfo the identity info of the identity we want to link the application
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus Claim(ApplicationInfo& app,
                  const IdentityInfo& identityInfo);

    /**
     * \brief Retrieve the Currently installed Identity of the given application.
     *
     * \param[in] appInfo the application info of the application that we want to retrieve the certificate from.
     * \param[out] certificate the identity certificate to fill in.
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus GetIdentityCertificate(const ApplicationInfo& appInfo,
                                   qcc::IdentityCertificate& idCert) const;

    /**
     * \brief Install a given generated Identity on a specific application.
     *
     * \param[in] app the application info of the application that we need to add the RoT to
     * \param[in] id the identity info describing owner/user of the application
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus InstallIdentity(const ApplicationInfo& app,
                            const IdentityInfo& id);

    /**
     * \brief Get the public key of this security manager.
     *
     * \retval publicKey the public key that of this security manager.
     */
    const qcc::ECCPublicKey& GetPublicKey() const;

    /**
     * \brief Get a list of all Applications that were discovered using About.
     *
     * \param[in] acs the claim status of the requested applications. If UNKOWN then all are returned.
     *
     * \retval vector<ApplicationInfo> on success
     * \retval empty-vector otherwise
     */
    std::vector<ApplicationInfo> GetApplications(
        ajn::PermissionConfigurator::ClaimableState acs = ajn::PermissionConfigurator::STATE_UNKNOWN) const;

    /**
     * \brief Register a listener that is called-back whenever the application info is changed.
     *
     * \param[in] al ApplicationListener listener on application changes
     */
    void RegisterApplicationListener(ApplicationListener* al);

    /**
     * \brief Unregister a previously registered listener on application info changes.
     *
     * \param[in] al ApplicationListener listener on application changes
     */
    void UnregisterApplicationListener(ApplicationListener* al);

    /**
     * \brief Get the application info based on a one with a given busName.
     *
     * \param[in, out] ai the application info to be filled in. Only the busName field is required
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus GetApplication(ApplicationInfo& ai) const;

    /**
     * \brief Add a Guild to be managed.
     *
     * \param[in] guildInfo the info of a given guild
     * \param[in] update a boolean to allow/deny guild info overwriting if the guild already exists
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus StoreGuild(const GuildInfo& guildInfo,
                       const bool update = false);

    /**
     * \brief Remove a previously managed Guild.
     *
     * \param[in] guildId the identifier of a given Guild
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus RemoveGuild(const qcc::GUID128& guildId);

    /**
     * \brief Get the information pertaining to a managed Guild.
     *
     * \param[in, out] guildInfo info of a given Guild. Only the Guild ID (GUID) is mandatory for input
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus GetGuild(GuildInfo& guildInfo) const;

    /**
     * \brief Get all information pertaining to all managed Guilds.
     *
     * \param[in, out] guildsInfo all info about all Guilds
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus GetManagedGuilds(std::vector<GuildInfo>& guildsInfo) const;

    /**
     * \brief Add an Identity info to be persistently stored.
     *
     * \param[in] identityInfo the info of a given identity that needs to be stored
     * \param[in] update a flag to specify whether identity info should be updated if the identity already exists
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus StoreIdentity(const IdentityInfo& identityInfo,
                          const bool update = false);

    /**
     * \brief Remove the stored information pertaining to a given Identity.
     *
     * \param[in] idInfoId the identifier of a given identity
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus RemoveIdentity(const qcc::GUID128& idInfoId);

    /**
     * \brief Get the info stored for a Identity Guild.
     *
     * \param[in, out] idInfo info of a given Identity. Only the GUID is mandatory for input
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus GetIdentity(IdentityInfo& idInfo) const;

    /**
     * \brief Get the info of all managed Identities.
     *
     * \param[in, out] identityInfos all info of currently managed Identities
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus GetManagedIdentities(std::vector<IdentityInfo>& identityInfos) const;

    /**
     * \brief Install a membership certificate on the application, making it a member of a specific guild.
     *
     * \param[in] appInfo the application that should become a member of the specific guild.
     * \param[in] guildInfo the guild to which the application should be added.
     * \param[in] authorizationData A permission policy containing the rules for the membership or NULL to use the
     *  manifest of the application.
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus InstallMembership(const ApplicationInfo& appInfo,
                              const GuildInfo& guildInfo,
                              const PermissionPolicy* authorizationData = NULL);

    /**
     * \brief Remove an application from a guild. Revoking its guild membership.
     *
     * \param[in] appInfo the application that should be removed from the specified guild.
     * \param[in] guildInfo the guild from which the application should be removed.
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus RemoveMembership(const ApplicationInfo& appInfo,
                             const GuildInfo& guildInfo);

    /**
     * \brief Install a policy on an application. This method does not persist
     * the policy locally unless the installation is successful on the remote application.
     *
     * \param[in] appInfo the application on which the policy should be
     * installed.
     * \param[in] policy  the policy that needs to be installed on the
     * application.
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus InstallPolicy(const ApplicationInfo& appInfo,
                          PermissionPolicy& policy);

    /**
     * \brief Retrieve the policy of an  application.
     *
     * \param[in] appInfo      the application from which the policy should be received.
     * \param[in, out] policy  the policy of the application
     * \param[in] remote       this flag determines from where the policy should be fetched, i.e., locally (persisted) or remotely.
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus GetPolicy(const ApplicationInfo& appInfo,
                      PermissionPolicy& policy,
                      bool remote);

    /**
     * \brief Removes any security configuration from a remote application. It
     * removes any installed Root of Trust, identity certificate, membership
     * certificate and policy. This method also removes any reference to the
     * application from local storage.
     *
     * \param[in] appInfo the application from which the security config will
     *                    be removed
     *
     * \retval ER_OK  on success
     * \retval others on failure
     */
    QStatus Reset(const ApplicationInfo& appInfo);

    ~SecurityManager();

  private:
    SecurityManagerImpl* securityManagerImpl;
};
}
}
#undef QCC_MODULE
#endif /* SECURITYMANAGER_H_ */
