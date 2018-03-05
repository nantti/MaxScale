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
#include <sstream>
#include <fstream>


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
            {"time-window", MXS_MODULE_PARAM_INT},
            {"events",      MXS_MODULE_PARAM_STRING},
            {"file-type",   MXS_MODULE_PARAM_STRING},
            {"file-mode",   MXS_MODULE_PARAM_STRING},
            {"report-file", MXS_MODULE_PARAM_STRING},
            {"totals-file", MXS_MODULE_PARAM_STRING},
            { MXS_END_MODULE_PARAMS }
        }
    };

    return &info;
}

static const MXS_ENUM_VALUE option_values[] =
{
    {"text",   stm_counter::Config::Text},
    {"json",   stm_counter::Config::Json},
    {"create", stm_counter::Config::Create},
    {"append", stm_counter::Config::Append}
};


namespace stm_counter
{
char commaToSpace(char ch)
{
    if (ch == ',') { return ' '; }
    return ch;
}

CounterFilter::CounterFilter(MXS_CONFIG_PARAMETER *params)
{
    int timeWindow = config_get_integer(params, "time-window");
    std::string event = config_get_string(params, "events");
    _config.fileType = static_cast<Config::FileType>
                       (config_get_enum(params, "file-type", option_values));
    _config.fileMode = static_cast<Config::FileMode>
                       (config_get_enum(params, "file-mode", option_values));
    _config.reportFile = config_get_string(params, "report-file");
    _config.totalsFile = config_get_string(params, "totals-file");

    if (!timeWindow) { timeWindow = 60; }
    _config.timeWindow = base::Duration(std::chrono::seconds(timeWindow));

    if (!event.empty())
    {
        // accept comma and space separarated strings
        std::transform(event.begin(), event.end(), event.begin(), commaToSpace);
        std::transform(event.begin(), event.end(), event.begin(), ::toupper);
        std::istringstream is(event);
        while (is >> event) { _config.events.push_back(event); }
    }

    if (_config.fileType == -1) { _config.fileType = Config::Text; }
    if (_config.fileMode == -1) { _config.fileMode = Config::Append; }
}

CounterFilter::~CounterFilter()
{
}

CounterFilter* CounterFilter::create(const char* zName, char** pzOptions, MXS_CONFIG_PARAMETER* pParams)
{
    return new CounterFilter(pParams);
}

CounterSession* CounterFilter::newSession(MXS_SESSION* mxsSession)
{
    CounterSession* sess = new CounterSession(mxsSession, this);
    SessionData sd
    {
        sess,
        {
            mxsSession->ses_id,
            mxsSession->client_dcb->user,
            _config.timeWindow
        }
    };
    _sessionData.emplace_back(std::move(sd));
    sess->setSessionStats(&_sessionData.back().sessionStats);

    if (_sessionData.size() )
    {
        _nextReportTime = base::Clock::now() + _config.timeWindow;
    }

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
    // Should keep stats around until they expire. FIXME.
    CounterSession* stmSession = static_cast<CounterSession*>(session);
    // would have to measure this. If there are lots of new/close session events compared
    // to queries, a map might be better (but probably not since everything is movable).
    auto ite = std::find_if(_sessionData.begin(), _sessionData.end(),
                            BySessionSid(stmSession->sessionId()));
    if (ite == _sessionData.end())
    {
        MXS_ERROR("CounterFilter::closeSession: unknwon session closed.");
        return;
    }

    // Keep the stats as long as there is something in them
    ite->counterSession = 0;
}

// static
void CounterFilter::diagnostics(DCB* pDcb)
{
    base::StopWatch sw;
    std::ostringstream os;
    _fillReport(os);
    _fillTotalsReport(os);

    os << "Report creation time: " << sw.lap() << '\n';

    dcb_printf(pDcb, "%s\n", os.str().c_str());
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

void CounterFilter::statisticsChanged(CounterSession *)
{
    _report();
}

void CounterFilter::_fillReport(std::ostream &os)
{
    for (auto ite = _sessionData.begin(); ite != _sessionData.end(); ++ite)
    {
        ite->sessionStats.streamHumanReadable(os);
    }
}

void CounterFilter::_fillTotalsReport(std::ostream &os)
{
    streamTotalsHumanReadable(os, _sessionData);
}


void CounterFilter::_report()
{
    if (base::Clock::now() >= _nextReportTime)
    {
        _purge();
        _nextReportTime = base::Clock::now() + _config.timeWindow;
        if (!_config.reportFile.empty())
        {
            std::ofstream of(_config.reportFile, std::ios_base::app);
            _fillReport(of);
        }
        if (!_config.totalsFile.empty())
        {
            std::ofstream of(_config.totalsFile, std::ios_base::app);
            _fillTotalsReport(of);
        }
    }
}

struct NotExpired
{
    bool operator()(const SessionData& sd)
    {
        return sd.counterSession == 0 && sd.sessionStats.empty();
    }
};

void CounterFilter::_purge()
{
    auto ite = std::partition(_sessionData.begin(), _sessionData.end(), NotExpired());
    if (ite != _sessionData.begin()) // 'if' due to gcc 4.4 bug
    {
        _sessionData.erase(ite, _sessionData.end());
    }
}
} // stm_counter
