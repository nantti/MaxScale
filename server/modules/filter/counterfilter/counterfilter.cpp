/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#define MXS_MODULE_NAME "counterfilter"
#include "counterfilter.h"
#include <maxscale/utils.h>
#include <string>
#include <algorithm>


extern "C" MXS_MODULE* MXS_CREATE_MODULE()
{
    static MXS_MODULE info =
    {
        MXS_MODULE_API_FILTER,
        MXS_MODULE_IN_DEVELOPMENT,
        MXS_FILTER_VERSION,
        "Keeps statistics over sql statements.",
        "V1.0.0",
        RCAP_TYPE_CONTIGUOUS_INPUT,
        &stm_counter::CounterFilter::s_object,
        NULL, /* Process init. */
        NULL, /* Process finish. */
        NULL, /* Thread init. */
        NULL, /* Thread finish. */
        {
//                {"service", MXS_MODULE_PARAM_SERVICE, NULL, MXS_MODULE_OPT_REQUIRED},
            {"some-option", MXS_MODULE_PARAM_STRING},
            { MXS_END_MODULE_PARAMS }
        }
    };

    return &info;
}

namespace stm_counter
{

CounterFilter::CounterFilter()
{
}

CounterFilter::~CounterFilter()
{
}

CounterFilter* CounterFilter::create(const char* zName, char** pzOptions, MXS_CONFIG_PARAMETER* pParams)
{
    return new CounterFilter();
}

CounterSession* CounterFilter::newSession(MXS_SESSION* mxsSession)
{
    auto timeWindow = std::chrono::seconds(10); // FIXME read this from config

    CounterSession* sess = CounterSession::create(mxsSession);
    SessionData sd {sess,
        {
            mxsSession->ses_id,
            mxsSession->client_dcb->user,
            base::Duration(timeWindow)
        }};
    _sessionData.emplace_back(std::move(sd));
    sess->setSessionStats(&_sessionData.back().sessionStats);

    return sess;
}

namespace
{
struct BySessionSid
{
    uint64_t sid;
    BySessionSid(uint64_t s) : sid(s) {}
    bool operator()(const SessionData& ss)
    {
        return sid == ss.sessionStats.sessionId();
    }
};
}

void CounterFilter::closeSession(MXS_FILTER *, MXS_FILTER_SESSION *session)
{
    CounterSession* stmSession = static_cast<CounterSession*>(session);
    // would have to measure this. If there are lots of new/close session events compared
    // to queries, a map might be better (but probably not).
    auto ite = std::find_if(_sessionData.begin(), _sessionData.end(),
                            BySessionSid(stmSession->sessionId()));
    if (ite == _sessionData.end())
    {
        MXS_ERROR("CounterFilter::closeSession: unknwon session closed.");
        return;
    }

    _sessionData.erase(ite);
}

// static
void CounterFilter::diagnostics(DCB* pDcb)
{
    dcb_printf(pDcb, "StmCount Hello!\n");
}

// static
json_t* CounterFilter::diagnostics_json() const
{
    return NULL;
}

uint64_t CounterFilter::getCapabilities()
{
    return RCAP_TYPE_NONE;
}
} // stm_counter
