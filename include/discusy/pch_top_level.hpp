#include <string> // IWYU pragma: keep
#include <variant> // IWYU pragma: keep
#include <tuple> // IWYU pragma: keep
#include <cstdint> // IWYU pragma: keep
#include <optional> // IWYU pragma: keep
#include <thread> // IWYU pragma: keep
#include <unordered_map> // IWYU pragma: keep
#include <flat_map> // IWYU pragma: keep
#include <atomic> // IWYU pragma: keep
#include <chrono> // IWYU pragma: keep
#include <string_view> // IWYU pragma: keep
#include <mutex> // IWYU pragma: keep
#include <shared_mutex> // IWYU pragma: keep
#include <vector> // IWYU pragma: keep
#include <utility> // IWYU pragma: keep
#include <stop_token> // IWYU pragma: keep
#include <new> // IWYU pragma: keep
#include <memory> // IWYU pragma: keep
#include <array> // IWYU pragma: keep
#include <random> // IWYU pragma: keep
#include <queue> // IWYU pragma: keep
#include <deque> // IWYU pragma: keep
#include <stdexcept> // IWYU pragma: keep
#include <ranges> // IWYU pragma: keep
#include <version> // IWYU pragma: keep
#include <bitset> // IWYU pragma: keep
#include <unordered_set> // IWYU pragma: keep
#include <set> // IWYU pragma: keep
#include <concepts> // IWYU pragma: keep
#include <filesystem> // IWYU pragma: keep
#include <type_traits> // IWYU pragma: keep
#include <fstream> // IWYU pragma: keep
#include <span> // IWYU pragma: keep
#include <initializer_list> // IWYU pragma: keep
#include <charconv> // IWYU pragma: keep
#include <bit> // IWYU pragma: keep

#include <format> // IWYU pragma: keep
#ifdef DISCUSY_LOGGING
#include <print> // IWYU pragma: keep
#include <cstdio> // IWYU pragma: keep
#endif

#include <boost/asio.hpp> // IWYU pragma: keep
#include <boost/asio/experimental/parallel_group.hpp> // IWYU pragma: keep
#include <boost/asio/experimental/awaitable_operators.hpp> // IWYU pragma: keep
#include <boost/asio/experimental/promise.hpp> // IWYU pragma: keep
#include <boost/asio/experimental/use_promise.hpp> // IWYU pragma: keep
#include <boost/asio/stream_file.hpp> // IWYU pragma: keep
#include <boost/asio/ssl.hpp> // IWYU pragma: keep
#include <boost/asio/ssl/stream.hpp> // IWYU pragma: keep
#include <boost/asio/co_composed.hpp> // IWYU pragma: keep
#include <boost/beast/core.hpp> // IWYU pragma: keep
#include <boost/beast/websocket.hpp> // IWYU pragma: keep
#include <boost/beast/websocket/ssl.hpp> // IWYU pragma: keep
#include <boost/beast/http.hpp> // IWYU pragma: keep
#include <boost/url.hpp> // IWYU pragma: keep
#include <boost/asio/cancel_after.hpp> // IWYU pragma: keep

#include <glaze/glaze.hpp> // IWYU pragma: keep
#include <glaze/base64/base64.hpp> // IWYU pragma: keep

#include <zstd.h> // IWYU pragma: keep

#ifdef __linux__
#include <sys/prctl.h>
#endif

#ifdef DISCUSY_VOICE
#include <sodium.h> // IWYU pragma: keep
#include <opus.h> // IWYU pragma: keep
#include <dave/dave_interfaces.h> // IWYU pragma: keep
#include <mls/crypto.h> // IWYU pragma: keep
#include <boost/container/deque.hpp> // IWYU pragma: keep
#endif

#ifdef WIN32
#include <windows.h> // IWYU pragma: keep
#include <wincrypt.h> // IWYU pragma: keep
#endif

#include <usylibpp/strings.hpp> // IWYU pragma: keep
#include <usylibpp/types.hpp> // IWYU pragma: keep