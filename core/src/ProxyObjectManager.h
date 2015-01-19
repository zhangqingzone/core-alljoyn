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
#ifndef PROXYOBJECTMANAGER_H_
#define PROXYOBJECTMANAGER_H_
#include "ApplicationInfo.h"
#include <SecurityManagerConfig.h>

#include <qcc/String.h>

#include <alljoyn/Status.h>
#include <alljoyn/Session.h>
#include <alljoyn/BusAttachment.h>

namespace ajn {
namespace securitymgr {
class ProxyObjectManager :
    public SessionListener {
  public:
    enum SessionType {
        ECDHE_NULL,
        ECDHE_DSA,
        ECDHE_PSK
    };

    ProxyObjectManager(ajn::BusAttachment* ba,
                       const SecurityManagerConfig& config);

    ~ProxyObjectManager();

    QStatus MethodCall(const ApplicationInfo app,
                       const qcc::String methodName,
                       const MsgArg* args,
                       const size_t numArgs,
                       Message& replyMsg);

  private:

    ajn::BusAttachment* bus;

    const char* objectPath;
    const char* interfaceName;
    std::map<qcc::String, SessionType> methodToSessionType;

    /* ajn::SessionListener */
    virtual void SessionLost(ajn::SessionId sessionId,
                             SessionLostReason reason);

    bool IsPermissionDeniedError(QStatus status,
                                 Message& msg);

    /**
     * \brief Ask the ProxyObjectManager to provide a ProxyBusObject to given
     * application. Ask the ProxyObjectManager to provide a ProxyBusObject to
     * given application. For every GetProxyObject, a matching
     * ReleaseProxyObject must be done.
     * \param[in] the application to connect to
     * \param[in] the type of session required
     * \param[out] remoteObject the ProxyBusObject will be stored in this
     * pointer on success.
     */
    QStatus GetProxyObject(const ApplicationInfo appInfo,
                           SessionType type,
                           ajn::ProxyBusObject** remoteObject);

    /**
     * \brief releases the remoteObject.
     * \param[in] the object to be released.
     */
    QStatus ReleaseProxyObject(ajn::ProxyBusObject* remoteObject);
};
}
}

#endif /* PROXYOBJECTMANAGER_H_ */
