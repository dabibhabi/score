#include <score/core/exceptions.hpp>
#include <score/net/json_codec.hpp>

namespace score::net {

std::vector<std::uint8_t> JsonFrameCodec::encode(const Frame& /*frame*/) const {
    // TODO: implement
    throw score::NotImplementedError("score::net::JsonFrameCodec::encode");
}

void JsonFrameCodec::feed(const std::uint8_t* /*data*/, std::size_t /*len*/) {
    // TODO: implement
    throw score::NotImplementedError("score::net::JsonFrameCodec::feed");
}

std::optional<Frame> JsonFrameCodec::try_decode() {
    // TODO: implement
    throw score::NotImplementedError("score::net::JsonFrameCodec::try_decode");
}

}  // namespace score::net
