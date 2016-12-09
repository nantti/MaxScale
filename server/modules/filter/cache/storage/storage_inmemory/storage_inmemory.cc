/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl.
 *
 * Change Date: 2019-07-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#define MXS_MODULE_NAME "storage_inmemory"
#include <maxscale/cppdefs.hh>
#include <inttypes.h>
#include "../../cache_storage_api.h"
#include "inmemorystoragest.hh"
#include "inmemorystoragemt.hh"

namespace
{

bool initialize(uint32_t* pcapabilities)
{
    *pcapabilities = CACHE_STORAGE_CAP_ST;
    *pcapabilities = CACHE_STORAGE_CAP_MT;

    return true;
}

CACHE_STORAGE* createInstance(cache_thread_model_t model,
                              const char* zname,
                              uint32_t ttl,
                              uint32_t max_count,
                              uint64_t max_size,
                              int argc, char* argv[])
{
    ss_dassert(zname);

    if (max_count != 0)
    {
        MXS_WARNING("A maximum item count of %u specified, although 'storage_inMemory' "
                    "does not enforce such a limit.", (unsigned int)max_count);
    }

    if (max_size != 0)
    {
        MXS_WARNING("A maximum size of %lu specified, although 'storage_inMemory' "
                    "does not enforce such a limit.", (unsigned long)max_size);
    }

    InMemoryStorage* pStorage = NULL;

    switch (model)
    {
    case CACHE_THREAD_MODEL_ST:
        MXS_EXCEPTION_GUARD(pStorage = InMemoryStorageST::create(zname, ttl, argc, argv));
        break;

    default:
        ss_dassert(!true);
        MXS_ERROR("Unknown thread model %d, creating multi-thread aware storage.", (int)model);
    case CACHE_THREAD_MODEL_MT:
        MXS_EXCEPTION_GUARD(pStorage = InMemoryStorageST::create(zname, ttl, argc, argv));
        break;
    }

    if (pStorage)
    {
        MXS_NOTICE("Storage module created.");
    }

    return reinterpret_cast<CACHE_STORAGE*>(pStorage);
}

void freeInstance(CACHE_STORAGE* pinstance)
{
    delete reinterpret_cast<InMemoryStorage*>(pinstance);
}

cache_result_t getInfo(CACHE_STORAGE* pStorage,
                       uint32_t       what,
                       json_t**       ppInfo)
{
    ss_dassert(pStorage);

    cache_result_t result = CACHE_RESULT_ERROR;

    MXS_EXCEPTION_GUARD(result = reinterpret_cast<InMemoryStorage*>(pStorage)->get_info(what, ppInfo));

    return result;
}

cache_result_t getKey(CACHE_STORAGE* pstorage,
                      const char* zdefault_db,
                      const GWBUF* pquery,
                      CACHE_KEY* pkey)
{
    ss_dassert(pstorage);
    // zdefault_db may be NULL.
    ss_dassert(pquery);
    ss_dassert(pkey);

    cache_result_t result = CACHE_RESULT_ERROR;

    MXS_EXCEPTION_GUARD(result = reinterpret_cast<InMemoryStorage*>(pstorage)->get_key(zdefault_db,
                                                                                       pquery,
                                                                                       pkey));

    return result;
}

cache_result_t getValue(CACHE_STORAGE* pstorage,
                        const CACHE_KEY* pkey,
                        uint32_t flags,
                        GWBUF** ppresult)
{
    ss_dassert(pstorage);
    ss_dassert(pkey);
    ss_dassert(ppresult);

    cache_result_t result = CACHE_RESULT_ERROR;

    MXS_EXCEPTION_GUARD(result = reinterpret_cast<InMemoryStorage*>(pstorage)->get_value(*pkey,
                                                                                         flags,
                                                                                         ppresult));

    return result;
}

cache_result_t putValue(CACHE_STORAGE* pstorage,
                        const CACHE_KEY* pkey,
                        const GWBUF* pvalue)
{
    ss_dassert(pstorage);
    ss_dassert(pkey);
    ss_dassert(pvalue);

    cache_result_t result = CACHE_RESULT_ERROR;

    MXS_EXCEPTION_GUARD(result = reinterpret_cast<InMemoryStorage*>(pstorage)->put_value(*pkey, pvalue));

    return result;
}

cache_result_t delValue(CACHE_STORAGE* pstorage,
                        const CACHE_KEY* pkey)
{
    ss_dassert(pstorage);
    ss_dassert(pkey);

    cache_result_t result = CACHE_RESULT_ERROR;

    MXS_EXCEPTION_GUARD(result = reinterpret_cast<InMemoryStorage*>(pstorage)->del_value(*pkey));

    return result;
}

}

extern "C"
{

CACHE_STORAGE_API* CacheGetStorageAPI()
{
    static CACHE_STORAGE_API api =
        {
            initialize,
            createInstance,
            freeInstance,
            getInfo,
            getKey,
            getValue,
            putValue,
            delValue,
        };

    return &api;
}

}