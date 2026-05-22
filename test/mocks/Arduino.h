#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/Arduino.h — host-native Arduino mock for PlatformIO unit tests
//
// Provides the Arduino String class and essential free functions so that
// project source files can be compiled in a POSIX (native) environment
// without an attached microcontroller.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>

// POSIX time helpers (settimeofday, struct timeval)
#ifdef __unix__
#  include <sys/time.h>
#elif defined(__APPLE__)
#  include <sys/time.h>
#endif

// ── Integer type aliases (mirror Arduino's typedefs) ─────────────────────────
// These are already in <cstdint>; just make sure they're available without
// the `std::` prefix, as Arduino code expects them at global scope.
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::int32_t;
using std::int64_t;
using std::size_t;

// ── Timing stubs ──────────────────────────────────────────────────────────────
inline uint32_t _arduinoMockMillis = 0;

inline uint32_t millis() { return _arduinoMockMillis; }
inline void     delay(uint32_t ms) { _arduinoMockMillis += ms; }
inline void     setMillis(uint32_t ms) { _arduinoMockMillis = ms; }
inline void     advanceMillis(uint32_t ms) { _arduinoMockMillis += ms; }

// ── NTP / SNTP stub ───────────────────────────────────────────────────────────
inline void configTime(long /*gmtOffset*/, int /*daylightOffset*/,
                       const char* /*server1*/,
                       const char* /*server2*/ = nullptr,
                       const char* /*server3*/ = nullptr) {}

// ── Minimal Print base (needed for SDCardManager::readFileToStream) ───────────
class Print {
public:
    virtual size_t write(uint8_t) { return 0; }
    virtual size_t write(const uint8_t* /*buf*/, size_t /*sz*/) { return 0; }
    virtual size_t write(const char* s) {
        return s ? write(reinterpret_cast<const uint8_t*>(s), std::strlen(s)) : 0;
    }
    virtual ~Print() = default;
};

// ── Printable — base for objects that can serialise themselves to a Print ─────
class Printable {
public:
    virtual size_t printTo(Print&) const = 0;
    virtual ~Printable() = default;
};

// ── Stream — character-level I/O base (used by ArduinoJson & others) ─────────
class Stream : public Print {
public:
    virtual int    available()                             { return 0;  }
    virtual int    read()                                  { return -1; }
    virtual int    peek()                                  { return -1; }
    virtual size_t readBytes(uint8_t* /*buf*/, size_t /*n*/) { return 0; }
    virtual size_t readBytes(char* /*buf*/, size_t /*n*/)    { return 0; }
    virtual ~Stream() = default;
};

// ── PROGMEM / Flash-memory stubs (AVR / ESP progmem API) ─────────────────────
// On a native host everything is in RAM, so these are trivial no-ops /
// pointer dereferences.
#ifndef PROGMEM
#  define PROGMEM
#endif
#define pgm_read_byte(addr)   (*(const uint8_t*)(addr))
#define pgm_read_word(addr)   (*(const uint16_t*)(addr))
#define pgm_read_dword(addr)  (*(const uint32_t*)(addr))
#define pgm_read_float(addr)  (*(const float*)(addr))
#define pgm_read_ptr(addr)    (*(const void* const*)(addr))

// ArduinoJson uses __FlashStringHelper and F() for PROGMEM strings.
struct __FlashStringHelper {};
#ifndef F
#  define F(s) (s)
#endif

// ── Arduino String class ─────────────────────────────────────────────────────
//
// Wraps std::string and exposes the subset of the Arduino String API that
// the project source files actually use.
class String {
public:
    // Constructors
    String() = default;
    String(const char* s) : _s(s ? s : "") {}           // NOLINT implicit
    String(const char* s, unsigned int len) : _s(s, len) {}
    String(const String&)            = default;
    String(String&&) noexcept        = default;
    String& operator=(const String&) = default;
    String& operator=(String&&) noexcept = default;
    String& operator=(const char* s) { _s = s ? s : ""; return *this; }

    // Concatenation
    String& operator+=(const String& o) { _s += o._s; return *this; }
    String& operator+=(const char* s)   { if (s) _s += s; return *this; }
    String& operator+=(char c)          { _s += c;      return *this; }

    String operator+(const String& o) const { String r(*this); r += o; return r; }
    String operator+(const char* s)   const { String r(*this); r += s; return r; }
    String operator+(char c)          const { String r(*this); r += c; return r; }

    // Comparison
    bool operator==(const String& o) const { return _s == o._s; }
    bool operator==(const char* s)   const { return s ? _s == s : _s.empty(); }
    bool operator!=(const String& o) const { return !(*this == o); }
    bool operator!=(const char* s)   const { return !(*this == s); }
    bool operator<(const String& o)  const { return _s < o._s; }

    // Character access
    char operator[](unsigned int i) const {
        return (i < _s.size()) ? _s[i] : '\0';
    }

    // String predicates
    bool startsWith(const char* p) const {
        if (!p) return false;
        return _s.compare(0, std::strlen(p), p) == 0;
    }
    bool startsWith(const String& p) const {
        return _s.compare(0, p._s.size(), p._s) == 0;
    }
    bool endsWith(const char* s) const {
        if (!s) return false;
        size_t sl = std::strlen(s);
        return _s.size() >= sl && _s.compare(_s.size() - sl, sl, s) == 0;
    }
    bool endsWith(const String& s) const { return endsWith(s.c_str()); }

    // Search
    int indexOf(char c, unsigned int from = 0) const {
        auto pos = _s.find(c, from);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }
    int indexOf(const char* s, unsigned int from = 0) const {
        if (!s) return -1;
        auto pos = _s.find(s, from);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }
    int indexOf(const String& s, unsigned int from = 0) const {
        return indexOf(s.c_str(), from);
    }
    int lastIndexOf(char c) const {
        auto pos = _s.rfind(c);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }

    // Substring
    String substring(unsigned int from) const {
        if (from >= _s.size()) return String("");
        return String(_s.c_str() + from);
    }
    String substring(unsigned int from, unsigned int to) const {
        if (from >= _s.size() || from >= to) return String("");
        size_t len = std::min((size_t)(to - from), _s.size() - from);
        return String(_s.substr(from, len).c_str());
    }

    // Metadata
    unsigned int length()  const { return (unsigned int)_s.size(); }
    bool         isEmpty() const { return _s.empty(); }

    // In-place modification
    void trim() {
        const char* ws = " \t\r\n";
        size_t s = _s.find_first_not_of(ws);
        if (s == std::string::npos) { _s.clear(); return; }
        size_t e = _s.find_last_not_of(ws);
        _s = _s.substr(s, e - s + 1);
    }

    // Raw access
    const char* c_str() const { return _s.c_str(); }

    // concat() — used by ArduinoJson's ArduinoStringWriter
    bool concat(const String& s) { *this += s; return true; }
    bool concat(const char*   s) { *this += s; return true; }
    bool concat(char          c) { *this += c; return true; }

private:
    std::string _s;
};

// Non-member operator+ for (const char* + String)
inline String operator+(const char* lhs, const String& rhs) {
    String r(lhs ? lhs : "");
    r += rhs;
    return r;
}
inline bool operator==(const char* lhs, const String& rhs) {
    return rhs == lhs;
}
