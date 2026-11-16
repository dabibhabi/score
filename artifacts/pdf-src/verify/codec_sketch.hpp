// Reference sketch of a length-prefixed JSON codec (NOT repository code).
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>
#include <nlohmann/json.hpp>

struct Frame { nlohmann::json payload; };

class SketchJsonFrameCodec {
public:
    static constexpr std::uint32_t kMaxFrame = 1u << 20;  // 1 MiB

    std::vector<std::uint8_t> encode(const Frame& f) const {
        const std::string body = f.payload.dump();
        const auto n = static_cast<std::uint32_t>(body.size());
        std::vector<std::uint8_t> out{
            static_cast<std::uint8_t>(n >> 24), static_cast<std::uint8_t>(n >> 16),
            static_cast<std::uint8_t>(n >> 8),  static_cast<std::uint8_t>(n)};
        out.insert(out.end(), body.begin(), body.end());
        return out;
    }

    void feed(const std::uint8_t* data, std::size_t len) {
        buf_.insert(buf_.end(), data, data + len);
    }

    std::optional<Frame> try_decode() {
        if (buf_.size() - pos_ < 4) return std::nullopt;            // HEADER: need 4 bytes
        const std::uint8_t* p = buf_.data() + pos_;
        const std::uint32_t n = (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) |
                                (std::uint32_t{p[2]} << 8)  |  std::uint32_t{p[3]};
        if (n > kMaxFrame) throw std::length_error("frame too large");
        if (buf_.size() - pos_ - 4 < n) return std::nullopt;        // BODY: need n bytes
        Frame f{nlohmann::json::parse(p + 4, p + 4 + n)};
        pos_ += 4 + n;
        if (pos_ == buf_.size()) { buf_.clear(); pos_ = 0; }        // compact when drained
        return f;
    }

private:
    std::vector<std::uint8_t> buf_;
    std::size_t pos_ = 0;
};
