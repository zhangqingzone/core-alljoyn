/**
 * @file
 */

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

#include <qcc/platform.h>
#include <qcc/Mutex.h>
#include <qcc/String.h>
#include <qcc/Thread.h>
#include <qcc/Debug.h>

#include "ProtectedAuthListener.h"

#include <list>

#include <alljoyn/Status.h>

#define QCC_MODULE "ALLJOYN_AUTH"

using namespace ajn;
using namespace qcc;


static const uint32_t ASYNC_AUTH_TIMEOUT = (120 * 1000);

class AuthContext {
  public:
    AuthContext(AuthListener* listener, AuthListener::Credentials* credentials) : listener(listener), accept(false), credentials(credentials) { }

    AuthListener* listener;
    bool accept;
    AuthListener::Credentials* credentials;
    qcc::Event event;
};

class AsyncTracker {
  public:

    static AuthContext* Allocate(AuthListener* listener, AuthListener::Credentials* credentials)
    {
        AuthContext* context = new AuthContext(listener, credentials);
        QCC_VERIFY(ER_OK == self->lock.Lock(MUTEX_CONTEXT));
        self->contexts.push_back(context);
        QCC_VERIFY(ER_OK == self->lock.Unlock(MUTEX_CONTEXT));
        return context;
    }

    static bool Trigger(AuthContext* context, bool accept, AuthListener::Credentials* credentials)
    {
        bool found = false;
        QCC_VERIFY(ER_OK == self->lock.Lock(MUTEX_CONTEXT));
        for (std::list<AuthContext*>::iterator it = self->contexts.begin(); it != self->contexts.end(); ++it) {
            if (*it == context) {
                self->contexts.erase(it);
                context->accept = accept;
                if (accept && credentials && context->credentials) {
                    *context->credentials = *credentials;
                }
                /*
                 * Set the event to unblock the waiting thread
                 */
                context->event.SetEvent();
                found = true;
                break;
            }
        }
        QCC_VERIFY(ER_OK == self->lock.Unlock(MUTEX_CONTEXT));
        return found;
    }

    static void Release(AuthContext* context)
    {
        Trigger(context, false, NULL);
        delete context;
    }

    static void RemoveAll(AuthListener* listener)
    {
        QCC_VERIFY(ER_OK == self->lock.Lock(MUTEX_CONTEXT));
        for (std::list<AuthContext*>::iterator it = self->contexts.begin(); it != self->contexts.end();) {
            AuthContext* context = *it;
            if (context->listener == listener) {
                /*
                 * Set the event to unblock the waiting thread
                 */
                context->accept = false;
                context->event.SetEvent();
                it = self->contexts.erase(it);
            } else {
                ++it;
            }
        }
        QCC_VERIFY(ER_OK == self->lock.Unlock(MUTEX_CONTEXT));
    }

  private:
    std::list<AuthContext*> contexts;
    qcc::Mutex lock;

    static AsyncTracker* self;
    friend class ajn::ProtectedAuthListener;
};

AsyncTracker* AsyncTracker::self = NULL;

void ProtectedAuthListener::Init()
{
    AsyncTracker::self = new AsyncTracker();
}

void ProtectedAuthListener::Shutdown()
{
    delete AsyncTracker::self;
    AsyncTracker::self = NULL;
}

void ProtectedAuthListener::Set(AuthListener* authListener)
{
    lock.Lock(MUTEX_CONTEXT);
    /*
     * Clear the current listener to prevent any more calls to this listener.
     */
    AuthListener* goner = this->listener;
    this->listener = NULL;
    /*
     * Remove the listener unblocking any threads that might be waiting
     */
    if (goner) {
        AsyncTracker::RemoveAll(goner);
    }
    /*
     * Poll and sleep until the current listener is no longer in use.
     */
    while (refCount) {
        lock.Unlock(MUTEX_CONTEXT);
        qcc::Sleep(10);
        lock.Lock(MUTEX_CONTEXT);
    }
    /*
     * Now set the new listener
     */
    this->listener = authListener;
    lock.Unlock(MUTEX_CONTEXT);
}

bool ProtectedAuthListener::RequestCredentials(const char* authMechanism, const char* peerName, uint16_t authCount, const char* userName, uint16_t credMask, Credentials& credentials)
{
    bool ok = false;
    lock.Lock(MUTEX_CONTEXT);
    AuthListener* authListener = this->listener;
    ++refCount;
    lock.Unlock(MUTEX_CONTEXT);
    if (authListener) {
        AuthContext* context = AsyncTracker::Allocate(authListener, &credentials);
        /*
         * First try the asynch implementation
         */
        QStatus status = authListener->RequestCredentialsAsync(authMechanism, peerName, authCount, userName, credMask, (void*)context);
        if (status != ER_OK) {
            if (status == ER_NOT_IMPLEMENTED) {
                ok = authListener->RequestCredentials(authMechanism, peerName, authCount, userName, credMask, credentials);
            }
        } else {
            ok = (Event::Wait(context->event, ASYNC_AUTH_TIMEOUT) == ER_OK) && context->accept;
        }
        AsyncTracker::Release(context);
    }
    lock.Lock(MUTEX_CONTEXT);
    --refCount;
    lock.Unlock(MUTEX_CONTEXT);
    return ok;
}

bool ProtectedAuthListener::VerifyCredentials(const char* authMechanism, const char* peerName, const Credentials& credentials)
{
    bool ok = false;
    lock.Lock(MUTEX_CONTEXT);
    AuthListener* authListener = this->listener;
    ++refCount;
    lock.Unlock(MUTEX_CONTEXT);
    if (authListener) {
        AuthContext* context = AsyncTracker::Allocate(authListener, NULL);
        /*
         * First try the asynch implementation
         */
        QStatus status = authListener->VerifyCredentialsAsync(authMechanism, peerName, credentials, (void*)context);
        if (status != ER_OK) {
            if (status == ER_NOT_IMPLEMENTED) {
                ok = authListener->VerifyCredentials(authMechanism, peerName, credentials);
            }
        } else {
            ok = (Event::Wait(context->event, ASYNC_AUTH_TIMEOUT) == ER_OK) && context->accept;
        }
        AsyncTracker::Release(context);
    }
    lock.Lock(MUTEX_CONTEXT);
    --refCount;
    lock.Unlock(MUTEX_CONTEXT);
    return ok;
}

void ProtectedAuthListener::SecurityViolation(QStatus status, const Message& msg)
{
    lock.Lock(MUTEX_CONTEXT);
    AuthListener* authListener = this->listener;
    ++refCount;
    lock.Unlock(MUTEX_CONTEXT);
    if (authListener) {
        authListener->SecurityViolation(status, msg);
    }
    lock.Lock(MUTEX_CONTEXT);
    --refCount;
    lock.Unlock(MUTEX_CONTEXT);
}

void ProtectedAuthListener::AuthenticationComplete(const char* authMechanism, const char* peerName, bool success)
{
    lock.Lock(MUTEX_CONTEXT);
    AuthListener* authListener = this->listener;
    ++refCount;
    lock.Unlock(MUTEX_CONTEXT);
    if (authListener) {
        authListener->AuthenticationComplete(authMechanism, peerName, success);
    }
    lock.Lock(MUTEX_CONTEXT);
    --refCount;
    lock.Unlock(MUTEX_CONTEXT);
}

/*
 * Static handler for auth credential response
 */
QStatus AJ_CALL AuthListener::RequestCredentialsResponse(void* context, bool accept, Credentials& credentials)
{
    return AsyncTracker::Trigger((AuthContext*)context, accept, &credentials) ? ER_OK : ER_TIMEOUT;
}

/*
 * Static handler for auth credential verification
 */
QStatus AJ_CALL AuthListener::VerifyCredentialsResponse(void* context, bool accept)
{
    return AsyncTracker::Trigger((AuthContext*)context, accept, NULL) ? ER_OK : ER_TIMEOUT;
}
