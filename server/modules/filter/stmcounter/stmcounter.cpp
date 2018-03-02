/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#define MXS_MODULE_NAME "stmcounter"
#include "stmcounter.h"
#include <string>
#include <maxscale/utils.h>


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
        &stm_counter::StmCounter::s_object,
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

StmCounter::StmCounter()
{
}

StmCounter::~StmCounter()
{
}

StmCounter* StmCounter::create(const char* zName, char** pzOptions, MXS_CONFIG_PARAMETER* pParams)
{
    return new StmCounter();
}

StmSession* StmCounter::newSession(MXS_SESSION* pSession)
{
    return StmSession::create(pSession, this);
}

void StmCounter::closeSession(MXS_FILTER *, MXS_FILTER_SESSION *pData)
{

}

// static
void StmCounter::diagnostics(DCB* pDcb)
{
    dcb_printf(pDcb, "StmCount Hello!\n");
}

// static
json_t* StmCounter::diagnostics_json() const
{
    return NULL;
}

uint64_t StmCounter::getCapabilities()
{
    return RCAP_TYPE_NONE;
}
} // stm_counter
