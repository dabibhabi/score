#pragma once

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>

namespace score::net {

struct Frame {
    nlohmann::json payload;
};

/// Wire-format abstraction: encodes a Frame to bytes and incrementally
/// decodes bytes back into Frames. Connection/Server hold a FrameCodec by
/// interface, so swapping the v1 JSON codec for a hand-rolled binary codec
/// later only touches the codec implementation.
class FrameCodec {
public:
    virtual ~FrameCodec() = default;

    /// Serializes `frame` to bytes ready to write to a socket.
    [[nodiscard]] virtual std::vector<std::uint8_t> encode(const Frame& frame) const = 0;

    /// Appends newly-read bytes to the codec's internal buffer.
    virtual void feed(const std::uint8_t* data, std::size_t len) = 0;

    /// Pops one complete Frame from the internal buffer if available.
    virtual std::optional<Frame> try_decode() = 0;
};

}  // namespace score::net
