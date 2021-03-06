/******************************************************************************
 *    Copyright (c) Open Connectivity Foundation (OCF), AllJoyn Open Source
 *    Project (AJOSP) Contributors and others.
 *
 *    SPDX-License-Identifier: Apache-2.0
 *
 *    All rights reserved. This program and the accompanying materials are
 *    made available under the terms of the Apache License, Version 2.0
 *    which accompanies this distribution, and is available at
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Copyright (c) Open Connectivity Foundation and Contributors to AllSeen
 *    Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for
 *    any purpose with or without fee is hereby granted, provided that the
 *    above copyright notice and this permission notice appear in all
 *    copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 *    WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 *    WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 *    AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 *    DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 *    PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 *    TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 *    PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/AboutIconProxy.h>
#include <qcc/Debug.h>

#define QCC_MODULE "ALLJOYN_ABOUT"

using namespace ajn;

AboutIconProxy::AboutIconProxy(ajn::BusAttachment& bus, const char* busName, SessionId sessionId) :
    ProxyBusObject(bus, busName, org::alljoyn::Icon::ObjectPath, sessionId)
{
    QCC_DbgTrace(("AboutIcontClient::%s", __FUNCTION__));
    const InterfaceDescription* p_InterfaceDescription = bus.GetInterface(org::alljoyn::Icon::InterfaceName);
    QCC_ASSERT(p_InterfaceDescription);
    AddInterface(*p_InterfaceDescription);
}

QStatus AboutIconProxy::GetIcon(AboutIcon& icon) {
    QCC_DbgTrace(("AboutIcontClient::%s", __FUNCTION__));
    QStatus status = ER_OK;

    Message replyMsg(GetBusAttachment());
    status = MethodCall(org::alljoyn::Icon::InterfaceName, "GetContent", NULL, 0, replyMsg);
    if (status == ER_OK) {
        const ajn::MsgArg* returnArgs;
        size_t numArgs;
        replyMsg->GetArgs(numArgs, returnArgs);
        if (numArgs == 1) {
            status = icon.SetContent(returnArgs[0]);
            if (status != ER_OK) {
                return status;
            }
        } else {
            return ER_BUS_BAD_VALUE;
        }
    } else {
        return status;
    }

    status = MethodCall(org::alljoyn::Icon::InterfaceName, "GetUrl", NULL, 0, replyMsg);
    if (status == ER_OK) {
        const ajn::MsgArg* returnArgs;
        size_t numArgs;
        replyMsg->GetArgs(numArgs, returnArgs);
        if (numArgs == 1) {
            char* temp;
            status = returnArgs[0].Get("s", &temp);
            if (status == ER_OK) {
                icon.url.assign(temp);
            } else {
                return status;
            }
        } else {
            return ER_BUS_BAD_VALUE;
        }
    } else {
        return status;
    }

    MsgArg iconsProperties_arg;
    status = GetAllProperties(org::alljoyn::Icon::InterfaceName, iconsProperties_arg);
    if (ER_OK == status) {
        MsgArg* iconPropertiesValues;
        size_t numValues;
        status = iconsProperties_arg.Get("a{sv}", &numValues, &iconPropertiesValues);
        if (status != ER_OK) {
            return status;
        }
        for (size_t i = 0; i < numValues; ++i) {
            if (status == ER_OK) {
                if (iconPropertiesValues[i].v_dictEntry.key->v_string.str != NULL) {
                    if (strcmp("MimeType", iconPropertiesValues[i].v_dictEntry.key->v_string.str) == 0) {
                        icon.mimetype.assign(iconPropertiesValues[i].v_dictEntry.val->v_variant.val->v_string.str);
                        continue;
                    }
                    if (strcmp("Size", iconPropertiesValues[i].v_dictEntry.key->v_string.str) == 0) {
                        icon.contentSize = iconPropertiesValues[i].v_dictEntry.val->v_variant.val->v_uint32;
                        continue;
                    }
                }
            } else {
                return status;
            }
        }
    }

    return status;
}

QStatus AboutIconProxy::GetVersion(uint16_t& version) {
    QCC_DbgTrace(("AboutIcontClient::%s", __FUNCTION__));
    QStatus status = ER_OK;

    MsgArg arg;
    status = GetProperty(org::alljoyn::Icon::InterfaceName, "Version", arg);
    if (ER_OK == status) {
        version = arg.v_variant.val->v_int16;
    }
    return status;
}
