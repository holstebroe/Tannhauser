#ifndef TEARWASH_WAV_HPP
#define TEARWASH_WAV_HPP

// Minimal RIFF/WAVE reader (PCM 16/24/32, float 32, EXTENSIBLE) and float-32 writer for the
// offline calibration tools (never used by the plugins). Header-only so the out-of-tree
// oracle driver (tools/oracle) can share it.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace tearwash {

struct Audio {
    unsigned rate = 48000;
    std::vector<std::vector<float>> ch;   // ch[c][n]
    size_t frames() const { return ch.empty() ? 0 : ch[0].size(); }
};

namespace detail {
inline uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
inline uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
inline void put32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back(uint8_t(x >> (8 * i))); }
inline void put16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(uint8_t(x)); v.push_back(uint8_t(x >> 8)); }
}

inline bool readWav(const std::string& path, Audio& out, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { err = "cannot open " + path; return false; }
    std::vector<uint8_t> d;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
    std::fclose(f);
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) {
        err = path + ": not a RIFF/WAVE file"; return false;
    }
    unsigned fmt = 0, chans = 0, bits = 0;
    size_t pos = 12;
    const uint8_t* data = nullptr;
    size_t dataLen = 0;
    while (pos + 8 <= d.size()) {
        const uint32_t len = detail::le32(&d[pos + 4]);
        const uint8_t* body = &d[pos + 8];
        if (pos + 8 + len > d.size()) break;
        if (!std::memcmp(&d[pos], "fmt ", 4) && len >= 16) {
            fmt = detail::le16(body);
            chans = detail::le16(body + 2);
            out.rate = detail::le32(body + 4);
            bits = detail::le16(body + 14);
            if (fmt == 0xFFFE && len >= 26) fmt = detail::le16(body + 24);
        } else if (!std::memcmp(&d[pos], "data", 4)) {
            data = body; dataLen = len;
        }
        pos += 8 + len + (len & 1);
    }
    if (!data || chans == 0) { err = path + ": no fmt/data chunk"; return false; }
    const unsigned bytes = bits / 8;
    if (!((fmt == 1 && (bits == 16 || bits == 24 || bits == 32)) || (fmt == 3 && bits == 32))) {
        err = path + ": unsupported sample format"; return false;
    }
    const size_t frames = dataLen / (bytes * chans);
    out.ch.assign(chans, std::vector<float>(frames));
    for (size_t i = 0; i < frames; ++i) {
        for (unsigned c = 0; c < chans; ++c) {
            const uint8_t* p = data + (i * chans + c) * bytes;
            float v;
            if (fmt == 3) { uint32_t u = detail::le32(p); std::memcpy(&v, &u, 4); }
            else if (bits == 16) v = int16_t(detail::le16(p)) / 32768.0f;
            else if (bits == 24) v = (int32_t(uint32_t(p[0] << 8) | (p[1] << 16) | (uint32_t(p[2]) << 24)) >> 8) / 8388608.0f;
            else v = float(int32_t(detail::le32(p)) / 2147483648.0);
            out.ch[c][i] = v;
        }
    }
    return true;
}

inline bool writeWavFloat(const std::string& path, const Audio& a, std::string& err) {
    const unsigned chans = static_cast<unsigned>(a.ch.size());
    const size_t frames = a.frames();
    std::vector<uint8_t> v;
    const uint32_t dataLen = static_cast<uint32_t>(frames * chans * 4);
    v.insert(v.end(), { 'R', 'I', 'F', 'F' });
    detail::put32(v, 36 + dataLen);
    v.insert(v.end(), { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
    detail::put32(v, 16);
    detail::put16(v, 3);
    detail::put16(v, uint16_t(chans));
    detail::put32(v, a.rate);
    detail::put32(v, a.rate * chans * 4);
    detail::put16(v, uint16_t(chans * 4));
    detail::put16(v, 32);
    v.insert(v.end(), { 'd', 'a', 't', 'a' });
    detail::put32(v, dataLen);
    for (size_t i = 0; i < frames; ++i)
        for (unsigned c = 0; c < chans; ++c) {
            uint32_t u;
            std::memcpy(&u, &a.ch[c][i], 4);
            detail::put32(v, u);
        }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { err = "cannot write " + path; return false; }
    const bool ok = std::fwrite(v.data(), 1, v.size(), f) == v.size();
    std::fclose(f);
    if (!ok) err = "short write " + path;
    return ok;
}

} // namespace tearwash

#endif
