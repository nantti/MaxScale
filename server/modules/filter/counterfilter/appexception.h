/*
 * Copyright (c) Niclas Antti
 *
 * This software is released under the MIT License.
 */
#ifndef APP_EXCEPTION_H
#define APP_EXCEPTION_H

#include <stdexcept>
#include <sstream>

namespace base
{

// TODO Add back trace.
class AppException : public std::runtime_error
{
public:
    AppException(const std::string& msg, const std::string& file,
                 int line) :
        std::runtime_error(msg), _file(file), _line(line)
    {}
private:
    std::string _file;
    int _line;
};
} //base

#define EXCEPTION(Type) \
    struct Type : public base::AppException { \
        Type(const std::string& msg, const char* file, \
            int line) : \
            AppException(msg, file, line) {}}

#define THROW(Type, msg_str) {\
        std::ostringstream os; \
        os << __FILE__ << ':' << __LINE__ << '\n' << msg_str; \
        throw Type(os.str(), __FILE__, __LINE__);}

#endif // APP_EXCEPTION_H
