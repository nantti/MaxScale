/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */

#define MXS_MODULE_NAME "counterfilter"
#include "counterfilter.h"
#include "eventout.h"
#include <maxscale/utils.h>
#include <string>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <maxscale/json_api.h>
#include <maxscale/jansson.hh>


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
    {"create", std::ios_base::trunc},
    {"append", std::ios_base::app},
    {NULL}
};


namespace stm_counter
{
std::vector<std::string> csvParse(const std::string& str);

CounterFilter::CounterFilter(MXS_CONFIG_PARAMETER *params) : _shutdown(false)
{
    int timeWindow = config_get_integer(params, "time-window");
    std::string eventStr = config_get_string(params, "events");
    _config.fileType = static_cast<Config::FileType>
                       (config_get_enum(params, "file-type", option_values));
    _config.fileMode = static_cast<std::ios_base::openmode>
                       (config_get_enum(params, "file-mode", option_values));
    _config.reportFile = config_get_string(params, "report-file");
    _config.totalsFile = config_get_string(params, "totals-file");

    if (!timeWindow) { timeWindow = 60; }
    _config.timeWindow = base::Duration(std::chrono::seconds(timeWindow));

    if (!eventStr.empty() && eventStr != "*")
    {
        std::transform(eventStr.begin(), eventStr.end(),
                       eventStr.begin(), ::toupper);
        _config.eventFilter = csvParse(eventStr);
    }

    if (_config.fileType == -1) { _config.fileType = Config::Json; }
    if (_config.fileMode == -1) { _config.fileMode = std::ios_base::out; }

    // using the given file mode to open files for reading, truncates if 'create' is selected.
    if (!_config.reportFile.empty())
    {
        _config.reportFile = _addExtension(_config.reportFile);
        std::ofstream of(_config.reportFile.c_str(), _config.fileMode);
        if (!of)
        {
            MXS_ERROR("Could not open report-file %s. Check file permissions.",
                      _config.reportFile.c_str());
        }
    }
    if (!_config.totalsFile.empty())
    {
        _config.totalsFile = _addExtension(_config.totalsFile);
        std::ofstream of(_config.totalsFile.c_str(), _config.fileMode);
        if (!of)
        {
            MXS_ERROR("Could not open totals-file %s. Check file permissions.",
                      _config.reportFile.c_str());
        }
    }

    _notifier = std::thread(&CounterFilter::_notifier_run, this);
}

CounterFilter::~CounterFilter()
{
    _shutdown = true;
    _notifier.join();
}


// This is just a simple timer-callback, that should be using a
// condition variable, or an atomic for the _shutdown bool.
// Did not find timers in max?
void CounterFilter::_notifier_run()
{
    auto window = _config.timeWindow;
    base::TimePoint nextReportTime = base::Clock::now() + window;

    while (!_shutdown)
    {
        auto now = base::Clock::now();
        if (base::Clock::now() >= nextReportTime)
        {
            nextReportTime = base::Clock::now() + window;
            _report();
        }
        usleep(10000); // 10ms
    }
}

std::string CounterFilter::_addExtension(const std::string& fileName)
{
    std::string extension;
    switch (_config.fileType)
    {
    case Config::Text: extension = ".txt"; break;
    case Config::Json: extension = ".json"; break;
    }

    // add extension if it is not already there
    if (fileName.size() - fileName.rfind(extension) != extension.size())
    {
        return fileName + extension;
    }
    return fileName;
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
    {
        mxs::SpinLockGuard guard(_sessionLock);
        _sessionData.emplace_back(std::move(sd));
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
        return sid == ss.sessionStats->sessionId();
    }
};
}

void CounterFilter::sessionClose(CounterSession* session)
{
    CounterSession* counterSession = static_cast<CounterSession*>(session);
    mxs::SpinLockGuard guard(_sessionLock);
    // would have to measure this. If there are lots of new/close session events compared
    // to queries, a map might be better (but probably not).
    auto ite = std::find_if(_sessionData.begin(), _sessionData.end(),
                            BySessionSid(counterSession->sessionId()));
    if (ite == _sessionData.end())
    {
        MXS_ERROR("CounterFilter::closeSession: unknwon session closed.");
        return;
    }

    // Keep the stats as long as there is something in them
    // (check how session ids recycled).
    ite->counterSession = 0;
}

void CounterFilter::increment(SessionStats *stats, const std::string &event)
{
    mxs::SpinLockGuard guard(_sessionLock);
    stats->increment(event);
}

void CounterFilter::diagnostics(DCB * pDcb)
{
    base::StopWatch sw;
    std::ostringstream os;
    {
        _fillReport(os, Config::Text);
        _fillTotalsReport(os, Config::Text);
    }

    os << "Report creation took: " << sw.lap() << '\n';

    dcb_printf(pDcb, "%s\n", os.str().c_str());
}

json_t* CounterFilter::diagnostics_json() const
{
    return NULL;
}

uint64_t CounterFilter::getCapabilities()
{
    return RCAP_TYPE_NONE;
}

void CounterFilter::_fillReport(std::ostream & os, Config::FileType type)
{
    if (type == Config::Json)
    {
        mxs::SpinLockGuard guard(_sessionLock);
        dumpJson(os, _sessionData);
    }
    else
    {
        mxs::SpinLockGuard guard(_sessionLock);
        dump(os, _sessionData);
    }
}

void CounterFilter::_fillTotalsReport(std::ostream & os, Config::FileType type)
{
    if (type == Config::Json)
    {
        mxs::SpinLockGuard guard(_sessionLock);
        dumpJsonTotals(os, _sessionData);
    }
    else
    {
        mxs::SpinLockGuard guard(_sessionLock);
        dumpTotals(os, _sessionData);
    }
}

void CounterFilter::_report()
{
    _purge();
    if (!_config.reportFile.empty())
    {
        std::ostringstream os;
        _fillReport(os, _config.fileType); // has locks, file write outside
        std::string rep = os.str();
        if (!rep.empty()) // don't write an emlty file, keep the last stats.
        {
            std::ofstream of(_config.reportFile, _config.fileMode);
            of << os.str();
        }
    }
    if (!_config.totalsFile.empty())
    {
        std::ostringstream os;
        _fillTotalsReport(os, _config.fileType);  // has locks, file write outside
        std::string rep = os.str();
        if (!rep.empty()) // don't write an emlty file, keep the last stats.
        {
            std::ofstream of(_config.totalsFile, _config.fileMode);
            of << os.str();
        }
    }
}

struct NotExpired
{
    bool operator()(const SessionData& sd)
    {
        return sd.counterSession || !sd.sessionStats->empty();
    }
};

void CounterFilter::_purge()
{
    mxs::SpinLockGuard guard(_sessionLock);
    auto ite = std::partition(_sessionData.begin(), _sessionData.end(), NotExpired());
    _sessionData.erase(ite, _sessionData.end());
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
