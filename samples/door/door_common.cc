/******************************************************************************
 * Copyright (c) AllSeen Alliance. All rights reserved.
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

#include "door_common.h"

#include <alljoyn/PermissionPolicy.h>

#if defined(QCC_OS_GROUP_WINDOWS)
#include <Winsock2.h> // gethostname
#endif

using namespace ajn;
using namespace qcc;
using namespace std;

namespace sample {
namespace securitymgr {
namespace door {
bool DoorAuthListener::RequestCredentials(const char* authMechanism,
                                          const char* authPeer,
                                          uint16_t authCount,
                                          const char* userId,
                                          uint16_t credMask,
                                          Credentials& creds)
{
    QCC_UNUSED(authPeer);
    QCC_UNUSED(authCount);
    QCC_UNUSED(userId);
    QCC_UNUSED(credMask);

    printf("RequestCredentials %s\n", authMechanism);

    // allow ECDHE_NULL and  DSA sessions
    if (strcmp(authMechanism, KEYX_ECDHE_NULL) == 0 || strcmp(authMechanism, KEYX_ECDHE_DSA)) {
        creds.SetExpiration(100);          /* set the master secret expiry time to 100 seconds */
        return true;
    }
    return false;
}

bool DoorAuthListener::VerifyCredentials(const char* authMechanism, const char* authPeer, const Credentials& creds)
{
    QCC_UNUSED(authPeer);
    QCC_UNUSED(creds);

    printf("VerifyCredentials %s\n", authMechanism);
    return false;
}

void DoorAuthListener::AuthenticationComplete(const char* authMechanism, const char* authPeer, bool success)
{
    QCC_UNUSED(authPeer);
    QCC_UNUSED(success);

    printf("AuthenticationComplete %s\n", authMechanism);
}

Door::Door(BusAttachment* ba) :
    BusObject(DOOR_OBJECT_PATH), open(false)
{
    const InterfaceDescription* secPermIntf = ba->GetInterface(DOOR_INTERFACE);
    assert(secPermIntf);
    AddInterface(*secPermIntf, ANNOUNCED);

    /* Register the method handlers with the object */
    const MethodEntry methodEntries[] = {
        { secPermIntf->GetMember(DOOR_OPEN), static_cast<MessageReceiver::MethodHandler>(&Door::Open) },
        { secPermIntf->GetMember(DOOR_CLOSE), static_cast<MessageReceiver::MethodHandler>(&Door::Close) },
        { secPermIntf->GetMember(DOOR_GET_STATE), static_cast<MessageReceiver::MethodHandler>(&Door::GetState) }
    };
    QStatus status = AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
    if (ER_OK != status) {
        printf("Failed to register method handlers for the Door.\n");
    }

    stateSignal = secPermIntf->GetMember(DOOR_STATE_CHANGED);
}

void Door::SendDoorEvent(bool newState)
{
    QCC_UNUSED(newState);
/*
    MsgArg outArg;
    outArg.Set("b", &newState);
    Signal(nullptr, SESSION_ID_ALL_HOSTED, *stateSignal, &outArg, 1, 0, 0,  nullptr);
 */
}

void Door::ReplyWithBoolean(bool answer, Message& msg)
{
    MsgArg outArg;
    outArg.Set("b", &answer);

    if (ER_OK != MethodReply(msg, &outArg, 1)) {
        printf("ReplyWithBoolean: Error sending reply.\n");
    }
}

void Door::Open(const InterfaceDescription::Member* member, Message& msg)
{
    QCC_UNUSED(member);

    printf("Door Open called\n");
    if (open == false) {
        open = true;
        SendDoorEvent(true);
    }
    ReplyWithBoolean(true, msg);
}

void Door::Close(const InterfaceDescription::Member* member,
                 Message& msg)
{
    QCC_UNUSED(member);

    printf("Door Close called\n");
    if (open) {
        open = false;
        SendDoorEvent(true);
    }
    ReplyWithBoolean(true, msg);
}

QStatus Door::Get(const char* ifcName, const char* propName, MsgArg& val)
{
    printf("Door::Get(%s)@%s\n", propName, ifcName);

    if (strcmp(ifcName, DOOR_INTERFACE) == 0 && strcmp(propName, DOOR_STATE) == 0) {
        val.Set("b", open);
        return ER_OK;
    }
    return ER_BUS_NO_SUCH_PROPERTY;
}

void Door::GetState(const InterfaceDescription::Member* member,
                    Message& msg)
{
    QCC_UNUSED(member);

    printf("Door GetState called\n");
    ReplyWithBoolean(open, msg);
}

QStatus DoorCommon::CreateInterface()
{
    InterfaceDescription* doorIntf = nullptr;
    QStatus status = ba->CreateInterface(DOOR_INTERFACE, doorIntf, DOOR_INTF_SECURE ? AJ_IFC_SECURITY_REQUIRED : 0);
    if (ER_OK == status) {
        printf("Interface created.\n");
        doorIntf->AddMethod(DOOR_OPEN, nullptr, "b", "success");
        doorIntf->AddMethod(DOOR_CLOSE, nullptr, "b", "success");
        doorIntf->AddMethod(DOOR_GET_STATE, nullptr, "b", "state");
        doorIntf->AddSignal(DOOR_STATE_CHANGED, "b", "state");
        doorIntf->AddProperty(DOOR_STATE, "b", PROP_ACCESS_RW);
        doorIntf->Activate();
    } else {
        printf("Failed to create Secure PermissionMgmt interface.\n");
    }

    return status;
}

QStatus DoorCommon::SetAboutData()
{
    GUID128 appId;
    aboutData.SetAppId(appId.ToString().c_str());

    char buf[64];
    gethostname(buf, sizeof(buf));
    aboutData.SetDeviceName(buf);

    GUID128 deviceId;
    aboutData.SetDeviceId(deviceId.ToString().c_str());

    aboutData.SetAppName(appName);
    aboutData.SetManufacturer("QEO LLC");
    aboutData.SetModelNumber("1");
    aboutData.SetDescription(appName);
    aboutData.SetDateOfManufacture("2015-04-14");
    aboutData.SetSoftwareVersion("0.1");
    aboutData.SetHardwareVersion("0.0.1");
    aboutData.SetSupportUrl("http://www.alljoyn.org");

    if (!aboutData.IsValid()) {
        cerr << "Invalid about data." << endl;
        return ER_FAIL;
    }
    return ER_OK;
}

QStatus DoorCommon::AnnounceAbout()
{
    QStatus status = SetAboutData();
    if (ER_OK != status) {
        cerr << "Failed to set about data = " << QCC_StatusText(status) << endl;
    }

    status = aboutObj->Announce(DOOR_APPLICATION_PORT, aboutData);
    if (ER_OK != status) {
        cerr << "Announcing about failed with status = " << QCC_StatusText(status) << endl;
    }
    return status;
}

QStatus DoorCommon::Init(const char* keyStoreName, bool provider)
{
    QStatus status = CreateInterface();

    if (status != ER_OK) {
        return status;
    }

    status = ba->Start();
    if (status != ER_OK) {
        return status;
    }

    status = ba->Connect();
    if (status != ER_OK) {
        return status;
    }

    status = ba->EnablePeerSecurity(KEYX_ECDHE_DSA " " KEYX_ECDHE_NULL, new DoorAuthListener(), keyStoreName);
    if (status != ER_OK) {
        return status;
    }

    PermissionPolicy::Rule::Member* member = new  PermissionPolicy::Rule::Member[1];
    member->SetMemberName("*");
    uint8_t mask = (provider ? PermissionPolicy::Rule::Member::ACTION_PROVIDE | 0 // | 0 was added to avoid an undefined reference at link time
                    : PermissionPolicy::Rule::Member::ACTION_MODIFY | PermissionPolicy::Rule::Member::ACTION_OBSERVE);
    member->SetActionMask(mask);
    member->SetMemberType(PermissionPolicy::Rule::Member::NOT_SPECIFIED);

    PermissionPolicy::Rule* manifestRule = new PermissionPolicy::Rule[1];
    manifestRule->SetInterfaceName(DOOR_INTERFACE);
    manifestRule->SetMembers(1, member);
    status = ba->GetPermissionConfigurator().SetPermissionManifest(manifestRule, 1);
    if (status != ER_OK) {
        return status;
    }

    return status;
}

QStatus DoorCommon::Fini()
{
    // empty string as authMechanism to avoid resetting keyStore
    QStatus status = ba->EnablePeerSecurity("", nullptr, nullptr, true);
    if (status != ER_OK) {
        cerr << "Failed to disable peer security at destruction..." << endl;
    }

    delete aboutObj;
    aboutObj = nullptr;

    ba->Disconnect();
    ba->Stop();
    ba->Join();

    delete ba;
    ba = nullptr;

    return status;
}

DoorCommon::~DoorCommon()
{
}
}
}
}
