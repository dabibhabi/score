#include <score/core/exceptions.hpp>
#include <score/net/json_codec.hpp>

namespace score::net {

namespace {
constexpr std::size_t kHeaderBytes = 4;
}  // namespace

std::vector<std::uint8_t> JsonFrameCodec::encode(const Frame& frame) const {
    const std::string body = frame.payload.dump();
    if (body.size() > kMaxFrameBytes) {
        throw DomainError("JsonFrameCodec::encode: frame exceeds maximum size");
    }

    const auto                len = static_cast<std::uint32_t>(body.size());
    std::vector<std::uint8_t> out;
    out.reserve(kHeaderBytes + body.size());
    out.push_back(static_cast<std::uint8_t>(len >> 24));
    out.push_back(static_cast<std::uint8_t>(len >> 16));
    out.push_back(static_cast<std::uint8_t>(len >> 8));
    out.push_back(static_cast<std::uint8_t>(len));
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

void JsonFrameCodec::feed(const std::uint8_t* data, std::size_t len) {
    buffer_.insert(buffer_.end(), data, data + len);
}

std::optional<Frame> JsonFrameCodec::try_decode() {
    if (buffer_.size() < kHeaderBytes) {
        return std::nullopt;
    }

    const std::uint32_t len = (static_cast<std::uint32_t>(buffer_[0]) << 24) |
                              (static_cast<std::uint32_t>(buffer_[1]) << 16) |
                              (static_cast<std::uint32_t>(buffer_[2]) << 8) | static_cast<std::uint32_t>(buffer_[3]);
    if (len > kMaxFrameBytes) {
        throw DomainError("JsonFrameCodec::try_decode: frame length exceeds maximum size");
    }
    if (buffer_.size() < kHeaderBytes + len) {
        return std::nullopt;
    }

    const auto body_begin = buffer_.begin() + kHeaderBytes;
    const auto body_end   = body_begin + len;
    // Consume the frame before parsing so a malformed payload cannot wedge the buffer.
    const std::string body(body_begin, body_end);
    buffer_.erase(buffer_.begin(), body_end);

    auto payload = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (payload.is_discarded()) {
        throw DomainError("JsonFrameCodec::try_decode: malformed JSON payload");
    }
    return Frame{std::move(payload)};
}

}  // namespace score::net
