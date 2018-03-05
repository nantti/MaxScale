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
    {"append", stm_counter::Config::Append},
    {NULL}
};


namespace stm_counter
{
std::vector<std::string> csvParse(const std::string& str);

CounterFilter::CounterFilter(MXS_CONFIG_PARAMETER *params)
{
    int timeWindow = config_get_integer(params, "time-window");
    std::string eventStr = config_get_string(params, "events");
    _config.fileType = static_cast<Config::FileType>
                       (config_get_enum(params, "file-type", option_values));
    _config.fileMode = static_cast<Config::FileMode>
                       (config_get_enum(params, "file-mode", option_values));
    _config.reportFile = config_get_string(params, "report-file");
    _config.totalsFile = config_get_string(params, "totals-file");

    if (!timeWindow) { timeWindow = 60; }
    _config.timeWindow = base::Duration(std::chrono::seconds(timeWindow));

    if (!eventStr.empty() && eventStr != "*")
    {
        // accept comma and space separarated strings
        std::transform(eventStr.begin(), eventStr.end(),
                       eventStr.begin(), ::toupper);
        _config.eventFilter = csvParse(eventStr);
    }

    if (_config.fileType == -1) { _config.fileType = Config::Text; }
    if (_config.fileMode == -1) { _config.fileMode = Config::Append; }

    if (!_config.reportFile.empty())
    {
        std::ifstream is(_config.reportFile.c_str());
        if (!is)
        {
            MXS_ERROR("Could not open report-file %s. Check file permissions.",
                      _config.reportFile.c_str());
        }
    }
    if (!_config.totalsFile.empty())
    {
        std::ifstream is(_config.totalsFile.c_str());
        if (!is)
        {
            MXS_ERROR("Could not open totals-file %s. Check file permissions.",
                      _config.reportFile.c_str());
        }
    }
}

CounterFilter::~CounterFilter()
{
}

CounterFilter* CounterFilter::create(const char* zName, char** pzOptions, MXS_CONFIG_PARAMETER * pParams)
{
    return new CounterFilter(pParams);
}

CounterSession* CounterFilter::newSession(MXS_SESSION * mxsSession)
{
    std::unique_ptr<SessionStats> stats(new SessionStats(mxsSession->ses_id,
                                                         mxsSession->client_dcb->user,
                                                         _config.timeWindow));
    // not paying for a shared ptr.
    CounterSession* sess = new CounterSession(mxsSession, this, stats.get());
    SessionData sd
    {
        sess,
        std::move(stats)
    };
    _sessionData.emplace_back(std::move(sd));
    _nextReportTime = base::Clock::now() + _config.timeWindow;

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
        return sid == ss.sessionStats->sessionId();
    }
};
}

void CounterFilter::closeSession(MXS_FILTER *, MXS_FILTER_SESSION * session)
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
void CounterFilter::diagnostics(DCB * pDcb)
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

void CounterFilter::_fillReport(std::ostream & os)
{
    for (auto ite = _sessionData.begin(); ite != _sessionData.end(); ++ite)
    {
        ite->sessionStats->streamHumanReadable(os);
    }
}

void CounterFilter::_fillTotalsReport(std::ostream & os)
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
        return sd.counterSession == 0 && sd.sessionStats->empty();
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
// One millionth implementation. Csv parse, but without escapes.
std::vector<std::string> csvParse(const std::string & str)
{
    const char * const whitespace = "\t\n\v\f\r ";
    const char * const quotes = "'\"";

    std::vector<std::string> ret;
    if (str.empty()) { return ret; }

    std::string::size_type b = 0, e = str.find_first_of(','), l = str.size();
    while (42)
    {
        auto bb = str.find_first_not_of(whitespace, b); // trim whitespace
        auto ee = str.find_last_not_of(whitespace, e - 1);
        bb = str.find_first_not_of(quotes, bb); // trim quotes
        ee = str.find_last_not_of(quotes, ee) + 1;

        if (b == bb && str[b] == ',') // first char is a comma.
        {
            ret.push_back("");
        }
        else if (bb == std::string::npos) // last char is a comma.
        {
            ret.push_back("");
            break;
        }
        else
        {
            ret.push_back(str.substr(bb, ee - bb));
        }

        if (e == std::string::npos) { break; }
        b = ++e;
        e = str.find_first_of(',', e);
    }

    return ret;
}

} // stm_counter
