//
//  net_common.h
//

#ifndef NET_COMMON_H
#define NET_COMMON_H

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

// since C++ 20
#include <version>

#ifdef __cpp_lib_filesystem
#include <filesystem>
namespace fs = std::filesystem;
#elif __cpp_lib_experimental_filesystem
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "no filesystem support ='("
#endif

namespace gpw {
namespace net {

inline const std::string
current_time () {
    // The std::chrono::system_clock class is the interface in C++ to get system-wide
    // real-time wall clock.  Most systems use Unix time, which is represented as seconds
    // past from 00:00:00 UTC on 1 January 1970 (an arbitrary date), called Unix epoch.
    // Note that leap seconds are ignored.  Thus Unix time is not truly an accurate
    // representation of UTC.
    //
    // Firstly, the now() method is called to return the current point in time.
    // The next method called is time_since_epoch to retrieve the amount of time
    // between *this and the clock's epoch, but it returns an std::chrono::duration
    // class object.  This object should call the count method to return the actual
    // number of ticks, and to represent it as milliseconds.  The result is cast
    // using duration_cast<milliseconds>.
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    auto now                  = system_clock::now();
    auto millisec_since_epoch = duration_cast<milliseconds> (now.time_since_epoch()).count();
    auto ms_str               = std::to_string (millisec_since_epoch % 1000);
    int  count_leading_zeros  = 3 - static_cast<int> (ms_str.length());

    std::stringstream strm;
    auto              now_c = system_clock::to_time_t (now);

    const char* fmt = "%H:%M:%S";

    strm << std::put_time (std::localtime (&now_c), fmt) << ".";
    for (unsigned i = 0; i != count_leading_zeros; i++) {
        strm << "0";
    }
    strm << ms_str;

    return strm.str();
}

inline const std::string
current_date () {
    std::time_t now = std::time (nullptr);
    std::tm     tstruct;
    char        buf[80];
    tstruct = *std::localtime (&now);

    const char* fmt = "%Y-%m-%d";

    std::strftime (buf, sizeof (buf), fmt, &tstruct);
    return buf;
}

inline std::string
_log_header () {
    return std::string{"[" + current_date() + " " + current_time() + "]"};
}

inline void
info (const std::string& str) {
    std::cout << _log_header() << "[ ] " << str << std::endl;
}

inline void
error (const std::string& str) {
    std::cerr << _log_header() << "[!] " << str << std::endl;
}

inline void
warn (const std::string& str) {
    std::cout << _log_header() << "[*] " << str << std::endl;
}

inline std::string
format (const char* s) {
    if (s == nullptr) return std::string{};

    std::stringstream ss;
    while (*s) {
        switch (*s) {
        case '{':
            if (*(s + 1) == '}') return std::string{};
            else if (*(s + 1) == '{') {
                ss << '{';
                s += 2;
            } else return std::string{};

            break;

        case '}':
            if (*(s + 1) == '}') {
                ss << '}';
                s += 2;
            } else return std::string{};

            break;

        default: ss << *s++;
        }
    }
    return ss.str();
}

template <typename T, typename... Args>
std::string
format (const char* s, T value, Args... args) {
    std::stringstream ss;

    while (s && *s) {
        switch (*s) {
        case '{':
            if (*(s + 1) == '}') {
                s += 2;
                ss << value;
                return ss.str() + format (s, args...);
            } else if (*(s + 1) == '{') {
                ss << '{';
                s += 2;
            } else return std::string{};

            break;

        case '}':
            if (*(s + 1) == '}') {
                ss << '}';
                s += 2;
            } else return std::string{};

            break;

        default: ss << *s++;
        }
    }
    return std::string{};
}

template <typename FormatString, typename... Args>
inline void
info (const FormatString& fmt, Args&&... args) {
    info (format (fmt, std::forward<Args> (args)...));
}

template <typename FormatString, typename... Args>
inline void
error (const FormatString& fmt, Args&&... args) {
    error (format (fmt, std::forward<Args> (args)...));
}

template <typename FormatString, typename... Args>
inline void
warn (const FormatString& fmt, Args&&... args) {
    warn (format (fmt, std::forward<Args> (args)...));
}

}  // namespace net
}  // namespace gpw

#endif
