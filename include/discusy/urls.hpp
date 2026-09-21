#pragma once

static_assert(true, "Clangd bug fix");
#pragma push_macro("API")
#undef API
#define API "10"

#pragma push_macro("REST_BASE_")
#undef REST_BASE_
#define REST_BASE_ "https://discord.com/api/v" API

#pragma push_macro("VOICE_API")
#undef VOICE_API
#define VOICE_API "8"

namespace discusy::urls {

inline constexpr auto GATEWAY_QUERY_PARAMS = "?v=" API "&encoding=json&compress=zstd-stream";

inline constexpr auto REST_BASE = REST_BASE_;
inline constexpr auto GET_BOT_PARAMS = REST_BASE_ "/gateway/bot";

inline constexpr auto VOICE_GATEWAY_QUERY_PARAMS = "?v=" VOICE_API;

}

#undef API
#pragma pop_macro("API")
#undef REST_BASE_
#pragma pop_macro("REST_BASE_")
#undef VOICE_API
#pragma pop_macro("VOICE_API")