#pragma once

#include <cstdint>
#include <score/net/codec.hpp>
#include <vector>

namespace score::net {

/// v1 wire format: a 4-byte big-endian length prefix followed by a
/// nlohmann::json payload serialized with dump(). Reuses the nlohmann_json
/// dependency already in the repo; a later binary codec implements the same
/// FrameCodec interface without requiring any Connection/Server changes.
class JsonFrameCodec : public FrameCodec {
public:
    [[nodiscard]] std::vector<std::uint8_t> encode(const Frame& frame) const override;
    void feed(const std::uint8_t* data, std::size_t len) override;
    std::optional<Frame> try_decode() override;

private:
    std::vector<std::uint8_t> buffer_;
};

}  // namespace score::net
