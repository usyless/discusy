#pragma once

#include <boost/asio.hpp>

namespace discusy::asio {

template <typename Token, typename... Args>
concept ctf = boost::asio::completion_token_for<Token, void(boost::system::error_code, Args...)>;

template <typename Token, typename... Args>
concept chf = boost::asio::completion_handler_for<Token, void(boost::system::error_code, Args...)>;

using ec_t = const boost::system::error_code;

}