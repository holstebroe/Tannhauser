#ifndef TEARWASH_RESAMPLER_HPP
#define TEARWASH_RESAMPLER_HPP

// Streaming arbitrary-ratio resampler between the host rate and the core rate (docs/tearwash/01
// §3): Kaiser-windowed sinc, cutoff at 0.46 of the lower rate, 16 zero crossings per side, the
// kernel tabulated at 512 points per zero crossing with linear interpolation. It also serves as
// the converters' anti-alias and anti-image filter. setup() allocates; push()/pull() do not.

#include <algorithm>
#include <cmath>
#include <vector>

namespace tearwash {

template <int NCH>
class StreamResampler {
public:
    // Delay of a resampler inRate → outRate in input samples (what setup() returns).
    static double delayFor(double inRate, double outRate) {
        return kZeros / (2.0 * 0.46 * std::min(1.0, outRate / inRate));
    }
    // inRate → outRate. Returns the delay in input samples.
    double setup(double inRate, double outRate) {
        step_ = inRate / outRate;
        cutoff_ = 0.46 * std::min(1.0, outRate / inRate);   // cycles per input sample
        half_ = delayFor(inRate, outRate);                   // kernel half-length, input samples
        const int n = kZeros * kRes + 2;
        table_.assign(n, 0.0f);
        const double beta = 8.0, i0b = besselI0(beta);
        for (int i = 0; i < n; ++i) {
            const double u = double(i) / kRes;                // zero crossings from the centre
            if (u >= kZeros) break;
            const double r = u / kZeros;
            const double sinc = u == 0.0 ? 1.0 : std::sin(kPi * u) / (kPi * u);
            table_[i] = static_cast<float>(2.0 * cutoff_ * sinc * besselI0(beta * std::sqrt(1.0 - r * r)) / i0b);
        }
        size_ = 1;
        while (size_ < static_cast<size_t>(2.0 * half_) + 8) size_ <<= 1;
        for (auto& b : buf_) b.assign(size_, 0.0f);
        reset();
        return half_;
    }
    void reset() {
        for (auto& b : buf_) std::fill(b.begin(), b.end(), 0.0f);
        written_ = 0;
        t_ = 0.0;
    }
    void push(const float* x) {
        const size_t w = written_ & (size_ - 1);
        for (int c = 0; c < NCH; ++c) buf_[c][w] = x[c];
        ++written_;
    }
    // Next output sample if enough input has arrived.
    bool pull(float* y) {
        const double need = t_ + half_;
        if (double(written_) - 1.0 < need) return false;
        const long i0 = static_cast<long>(std::ceil(t_ - half_));
        const long i1 = static_cast<long>(std::floor(t_ + half_));
        double acc[NCH] = {};
        const double scale = 2.0 * cutoff_ * kRes;           // input samples → table index
        for (long i = std::max(i0, 0L); i <= i1; ++i) {
            const double pos = std::fabs(double(i) - t_) * scale;
            const int k = static_cast<int>(pos);
            if (k >= kZeros * kRes) continue;
            const double f = pos - k;
            const double h = table_[k] + f * (table_[k + 1] - table_[k]);
            const size_t idx = static_cast<size_t>(i) & (size_ - 1);
            for (int c = 0; c < NCH; ++c) acc[c] += h * buf_[c][idx];
        }
        for (int c = 0; c < NCH; ++c) y[c] = static_cast<float>(acc[c]);
        t_ += step_;
        return true;
    }

private:
    static constexpr int kZeros = 16, kRes = 512;
    static constexpr double kPi = 3.14159265358979323846;
    static double besselI0(double x) {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
        return sum;
    }
    std::vector<float> table_;
    std::vector<float> buf_[NCH];
    size_t size_ = 1, written_ = 0;
    double step_ = 1.0, cutoff_ = 0.5, half_ = 16.0, t_ = 0.0;
};

} // namespace tearwash

#endif
