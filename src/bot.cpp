#include <discusy/discusy.hpp>

namespace discusy {

bot::bot(std::uint32_t threads) :
    cfg_{setup_cfg(threads)},
    io_ctx{static_cast<int>(cfg_.threads)},
    state_{
        .client{state_.ssl_ctx, io_ctx},
        .timers{io_ctx},
        .gateway_callbacks{io_ctx},
        .shards_ready{io_ctx},
        .shard_session_invalidated{io_ctx},
    },
    gateway_callbacks{state_.gateway_callbacks},
    timers{state_.timers},
    on_shards_ready{state_.shards_ready},
    client{state_.client},
    api{state_, cfg_}
    #ifndef DISCUSY_NO_CACHES
    ,caches{state_.gateway_callbacks}
    #endif
    #ifdef DISCUSY_VOICE
    ,voice{*this}
    #endif
    ,http_only_{true}
{
    construct();
}

void bot::assign_token(std::string_view token) {
    if (!http_only_) {
        throw std::runtime_error("Assign token can only be used for http only bots!");
    }
    cfg_.token.assign(token);
}

bot::bot(config cfg, bool add_voice_intent) :
    cfg_{setup_cfg(cfg, add_voice_intent)},
    io_ctx{static_cast<int>(cfg_.threads)},
    state_{
        .client{state_.ssl_ctx, io_ctx},
        .timers{io_ctx},
        .gateway_callbacks{io_ctx},
        .shards_ready{io_ctx},
        .shard_session_invalidated{io_ctx},
    },
    gateway_callbacks{state_.gateway_callbacks},
    timers{state_.timers},
    on_shards_ready{state_.shards_ready},
    client{state_.client},
    api{state_, cfg_}
    #ifndef DISCUSY_NO_CACHES
    ,caches{state_.gateway_callbacks}
    #endif
    #ifdef DISCUSY_VOICE
    ,voice{*this}
    #endif
{
    construct();
}

bot::bot(const std::filesystem::path& cfg_path, opt<send_event::update_presence>&& initial_presence, bool add_voice_intent) :
    cfg_{setup_cfg(cfg_path, std::move(initial_presence), add_voice_intent)},
    io_ctx{static_cast<int>(cfg_.threads)},
    state_{
        .client{state_.ssl_ctx, io_ctx},
        .timers{io_ctx},
        .gateway_callbacks{io_ctx},
        .shards_ready{io_ctx},
        .shard_session_invalidated{io_ctx},
    },
    gateway_callbacks{state_.gateway_callbacks},
    timers{state_.timers},
    on_shards_ready{state_.shards_ready},
    client{state_.client},
    api{state_, cfg_}
    #ifndef DISCUSY_NO_CACHES
    ,caches{state_.gateway_callbacks}
    #endif
    #ifdef DISCUSY_VOICE
    ,voice{*this}
    #endif
{
    construct();
}

void bot::run(const bool wait) {
    state_.application_id.clear();
    state_.gateway_url.clear();

    state_.ready_flag.reset();
    state_.ready_flag.emplace();

    shards_restarting_.store(false);
    shards_stop_requested.store(false);

    state_.sharding_required.reset();
    state_.sharding_required.emplace();

    state_.sharding_required_cb.reset();

    state_.fatal_error.reset();
    state_.fatal_error.emplace();

    state_.fatal_error_cb.reset();

    state_.shards_ready_counter.reset();

    setup_clients(wait);
    if (!http_only_) {
        start_shards();
    }

    client.start();

    if (wait) {
        worker();
    } else {
        worker_threads.emplace_back(&bot::worker, this);
    }
}

bool bot::send_request_guild_members(send_event::request_guild_members members) {
    if (!members.guild_id) return false;

    {
    const std::unique_lock lock{shards_mutex, std::chrono::milliseconds{500}};
    if (!lock.owns_lock()) return false;

    const auto total_shards = total_shards_.load();
    if (total_shards == 0 || shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
        const std::scoped_lock q_lock{pending_shard_operations_mtx_};
        pending_shard_operations_queue_.emplace_back([this, m = std::move(members)] mutable {
            this->send_request_guild_members(std::move(m));
        });
        return true;
    }

    const auto shard_id = members.guild_id.guild_shard_id(total_shards);

    if (shard_id >= total_shards) return false;

    return shards_[shard_id]->send_request_guild_members(members);
    }
}

bool bot::send_request_channel_info(send_event::request_channel_info info) {
    if (!info.guild_id) return false;

    {
    const std::unique_lock lock{shards_mutex, std::chrono::milliseconds{500}};
    if (!lock.owns_lock()) return false;

    const auto total_shards = total_shards_.load();
    if (total_shards == 0 || shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
        const std::scoped_lock q_lock{pending_shard_operations_mtx_};
        pending_shard_operations_queue_.emplace_back([this, i = std::move(info)] mutable {
            this->send_request_channel_info(std::move(i));
        });
        return true;
    }

    const auto shard_id = info.guild_id.guild_shard_id(total_shards);

    if (shard_id >= total_shards) return false;

    return shards_[shard_id]->send_request_channel_info(info);
    }
}

bool bot::request_soundboard_sounds(send_event::request_soundboard_sounds sounds) {
    if (sounds.guild_ids.empty()) return false;

    bool success = true;

    {
    const std::unique_lock lock{shards_mutex, std::chrono::milliseconds{500}};
    if (!lock.owns_lock()) return false;

    const auto total_shards = total_shards_.load();
    if (total_shards == 0 || shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
        const std::scoped_lock q_lock{pending_shard_operations_mtx_};
        pending_shard_operations_queue_.emplace_back([this, s = std::move(sounds)] mutable {
            this->request_soundboard_sounds(std::move(s));
        });
        return true;
    }

    std::vector<send_event::request_soundboard_sounds> shard_requests(total_shards);
    for (const auto& id : sounds.guild_ids) {
        const auto shard_id = id.guild_shard_id(total_shards);
        if (shard_id >= total_shards) return false;

        shard_requests[shard_id].guild_ids.emplace_back(id);
    }

    for (std::decay_t<decltype(total_shards)> i = 0; i < total_shards; ++i) {
        const auto& request = shard_requests[i];
        if (request.guild_ids.empty()) continue;

        success &= shards_[i]->send_request_soundboard_sounds(request);
    }
    }

    return success;
}

bool bot::set_presence(send_event::update_presence presence) {
    send_event::update_presence_payload const payload{presence};
    std::string encoded;
    if (gateway_protocol::write(payload, encoded, json_logger)) return false;

    {
    const std::unique_lock lock{shards_mutex, std::chrono::milliseconds{500}};
    if (!lock.owns_lock()) return false;

    state_.presence.store(std::make_shared<const send_event::update_presence>(presence), std::memory_order_release);

    const auto total_shards = total_shards_.load();
    if (total_shards == 0 || shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
        const std::scoped_lock q_lock{pending_shard_operations_mtx_};
        pending_shard_operations_queue_.emplace_back([this, p = std::move(presence)] mutable {
            this->set_presence(std::move(p));
        });
        return true;
    }

    for (const auto& shard : shards_) {
        shard->send_text_ratelimited(encoded);
    }
    }

    return true;
}

void bot::stop() {
    if (shards_stop_requested.exchange(true)) return;

    timers.clear();

    if (!http_only_) {
        stop_shards();
    }
    stop_workers();
    client.stop();
}

bot::~bot() {
    stop();
    worker_threads.clear();
}

std::string bot::get_application_id() {
    const std::shared_lock lock{this->state_.details_mtx};
    return this->state_.application_id;
}

snowflake bot::get_user_id() {
    const std::shared_lock lock{this->state_.details_mtx};
    return this->state_.me.id;
}

discusy::user::user bot::get_me() {
    const std::shared_lock lock{state_.details_mtx};
    return this->state_.me;
}

bool bot::update_voice_state(send_event::update_voice_state state) {
    if (!state.guild_id) return false;

    {
    const std::unique_lock lock{shards_mutex, std::chrono::milliseconds{500}};
    if (!lock.owns_lock()) return false;

    const auto total_shards = total_shards_.load();
    if (total_shards == 0 || shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
        const std::scoped_lock q_lock{pending_shard_operations_mtx_};
        pending_shard_operations_queue_.emplace_back([this, s = state] mutable {
            this->update_voice_state(s);
        });
        return true;
    }

    const auto shard_id = state.guild_id.guild_shard_id(total_shards);

    if (shard_id >= total_shards) return false;

    return shards_[shard_id]->send_update_voice_state(state);
    }
}

config bot::setup_cfg(std::uint32_t threads) {
    config conf;
    conf.threads = (threads < 1) ? 1 : threads;
    return conf;
}

config bot::setup_cfg(config& conf, [[maybe_unused]] bool add_voice_intent) {
    conf.threads = std::max<std::uint32_t>(conf.threads, 1);
    #ifdef DISCUSY_VOICE
    if (add_voice_intent) conf.intents |= discusy::voice::client<bot>::required_intents;
    #endif
    return conf;
}

config bot::setup_cfg(const std::filesystem::path& cfg_path, opt<send_event::update_presence>&& initial_presence, bool add_voice_intent) {
    config conf;

    std::ifstream data_file(cfg_path);
    if (!data_file.is_open()) {
        throw std::runtime_error("Failed to open config path");
    }
    glz::basic_istream_buffer<std::ifstream> buffer(data_file);

    if (json::parse_json<json::glz_opts_non_minified_error_missing>(conf, buffer, json_logger)) {
        throw std::runtime_error("Failed to parse json from config path");
    }

    if (initial_presence) {
        conf.initial_presence = std::move(initial_presence);
    }
    return setup_cfg(conf, add_voice_intent);
}

void bot::construct() {
    state_.bot_ptr = this;
    state_.gateway_callbacks.set_bot(this);

    cfg_.threads = std::max<std::uint32_t>(cfg_.threads, 1);
    if (cfg_.initial_presence) {
        state_.presence.store(std::make_shared<const send_event::update_presence>(std::move(*cfg_.initial_presence)), std::memory_order_release);
    }
    if (cfg_.max_pool_size) {
        client.set_max_pool_size(*cfg_.max_pool_size);
    }
    #ifdef DISCUSY_VOICE
    if (cfg_.audio_threads) {
        ctx::audio_runtime::configure(*cfg_.audio_threads);
    }
    #endif

    // set up ssl ctx
    state_.ssl_ctx.set_default_verify_paths();
    state_.ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
    state_.ssl_ctx.set_options(
                    boost::asio::ssl::context::default_workarounds |
                    boost::asio::ssl::context::no_sslv2 |
                    boost::asio::ssl::context::no_sslv3);
    ssl::setup_ssl_context(state_.ssl_ctx);

    #ifndef DISCUSY_NO_CACHES
    if (!http_only_) {
        caches.attach();
    }
    #endif

    if (!http_only_ && on_user_update_id_ == 0) {
        on_user_update_id_ = state_.gateway_callbacks.on_user_update.listen_system([this](const recieve_event::user_update& u) -> bool {
            const std::scoped_lock lock{state_.details_mtx};
            state_.me = u;
            return true;
        });
    }
}

void bot::worker() {
    while (true) {
        try {
            io_ctx->run();
            break;
        }
        #ifdef DISCUSY_LOGGING
        catch (const std::exception& e) {
            log("Io context worker error: {}", e.what());
            if (io_ctx->stopped()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            io_ctx->restart();
        }
        #endif
        catch (...) {
            #ifdef DISCUSY_LOGGING
            log("Io context worker unknown error");
            #endif
            if (io_ctx->stopped()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            io_ctx->restart();
        }
    }
}

void bot::sharding_required() {
    if (!shards_restarting_.exchange(true)) {
        stop_shards();

        state_.sharding_required.reset();
        state_.sharding_required.emplace();
        state_.sharding_required_cb.reset();

        shards_stop_requested.store(false);
        start_shards();
    }
}

void bot::start_shards() {
    #ifdef DISCUSY_LOGGING
    log("Starting shards");
    #endif

    shards_stop_requested.store(false);

    io_ctx.co_launch_detached([this] -> coro::awaitable<void> {
        auto [ec, resp] = co_await api.get_gateway_bot(token::as_tuple(token::deferred));

        if (ec || !resp) {
            #ifdef DISCUSY_LOGGING
            log("Failed to get bot params: {}", resp.error());
            #endif
            this->stop();
            co_return;
        }

        const auto& bot_info = resp.value();

        state_.gateway_url = ulp::str::concat_strings(bot_info.url, "/", urls::GATEWAY_QUERY_PARAMS);

        #ifdef DISCUSY_LOGGING
        log("Total shards: {}", bot_info.shards);
        log("Remaining launches: {}", bot_info.session_start_limit.remaining);
        log("Launches reset after: {}ms", bot_info.session_start_limit.reset_after);
        log("Gateway URL: {}", state_.gateway_url);
        #endif

        if (bot_info.session_start_limit.remaining < bot_info.shards) {
            #ifdef DISCUSY_LOGGING
            log("Not enough remaining session starts. Waiting {}ms before launching.",
                bot_info.session_start_limit.reset_after);
            #endif

            ctx::io_context::executor_timer_t timer{io_ctx.executor_, std::chrono::milliseconds{bot_info.session_start_limit.reset_after}};
            co_await timer.async_wait();
            start_shards();
            co_return;
        }

        const auto max_concurrency = bot_info.session_start_limit.max_concurrency > 0
                                                    ? bot_info.session_start_limit.max_concurrency
                                                    : 1;

        #ifdef DISCUSY_LOGGING
        log("Shard start concurrency: {}", max_concurrency);
        #endif

        state_.sharding_required_cb.emplace(state_.sharding_required->get_token(), [this] {
            #ifdef DISCUSY_LOGGING
            log("Sharding required callback");
            #endif
            io_ctx.post([this] {
                sharding_required();
            });
        });

        state_.fatal_error_cb.emplace(state_.fatal_error->get_token(), [this] {
            #ifdef DISCUSY_LOGGING
            log("Fatal error callback");
            #endif
            io_ctx.post([this] {
                stop();
            });
        });
        const auto total_shards = bot_info.shards;
        {
        const std::scoped_lock lock{shards_mutex};
        total_shards_.store(total_shards);
        shards_.reserve(total_shards);
        }

        state_.shards_ready_counter.emplace(total_shards, [this] {
            io_ctx.post([this] {
                std::vector<
                #if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
                    std::move_only_function<void()>
                #else
                    std::function<void()>
                #endif
                > to_execute;
                {
                const std::scoped_lock q_lock{pending_shard_operations_mtx_};
                to_execute = std::move(pending_shard_operations_queue_);
                pending_shard_operations_queue_.clear();
                }
                for (auto& task : to_execute) {
                    std::invoke(std::move(task));
                }
                auto me = get_me();
                #ifdef DISCUSY_VOICE
                voice.set_id(me.id);
                #endif
                state_.shards_ready.fire(std::move(me));
            });
        });

        for (decltype(bot_info.shards) i = 0; i < total_shards; i += max_concurrency) {
            if (shards_stop_requested.load()) break;

            const bool is_last_iteration = (i + max_concurrency >= total_shards);

            {
            const std::scoped_lock lock{shards_mutex};
            const auto upto = i + max_concurrency;
            for (decltype(bot_info.shards) j = i; (j < upto) && (j < total_shards); ++j) {
                if (shards_stop_requested.load()) break;

                #ifdef DISCUSY_LOGGING
                log("Starting shard: {}", j);
                #endif

                auto s = std::make_shared<shard>(io_ctx, j, total_shards, cfg_, state_);
                s->start();
                shards_.emplace_back(std::move(s));

                if (is_last_iteration && !shards_stop_requested.load()) {
                    shards_restarting_.store(false);
                    shards_stop_requested.store(false);
                }
            }
            }

            if (shards_stop_requested.load()) break;

            if (!is_last_iteration) {
                ctx::io_context::executor_timer_t timer{io_ctx.executor_, std::chrono::seconds{5}};
                co_await timer.async_wait();
            }
        }
        co_return;
    });
}

void bot::setup_clients(const bool wait) {
    worker_threads.clear();

    if (io_ctx->stopped()) {
        io_ctx->restart();
    }

    work_guard.emplace(boost::asio::make_work_guard(io_ctx.io_ctx_));

    const auto threads = cfg_.threads - 1;

    worker_threads.reserve(wait ? threads : cfg_.threads);

    for (std::remove_cvref_t<decltype(threads)> i = 0; i < threads; ++i) {
        worker_threads.emplace_back(&bot::worker, this);
    }
}

void bot::stop_shards() {
    #ifdef DISCUSY_LOGGING
    log("Stopping shards");
    #endif
    shards_stop_requested.store(true);
    {
    const std::scoped_lock lock{shards_mutex};
    total_shards_.store(0);
    const bool io_stopped = io_ctx->stopped();
    for (auto& shard_ptr : shards_) {
        if (io_stopped) {
            shard_ptr->shutdown();
            shard_ptr.reset();
            continue;
        }

        const auto strand = shard_ptr->strand_;
        boost::asio::post(strand, [this, shard_ptr = std::move(shard_ptr)] mutable {
            auto payloads = shard_ptr->extract_pending_payloads();

            if (!payloads.empty()) {
                const std::scoped_lock q_lock{this->pending_shard_operations_mtx_};
                for (auto& payload : payloads) {
                    this->pending_shard_operations_queue_.emplace_back(
                        [this, p = std::move(payload)] mutable {
                            this->route_evacuated_payload(std::move(p));
                        }
                    );
                }
            }
            shard_ptr->shutdown();
            shard_ptr.reset();
        });
    }
    shards_.clear();
    }
}

void bot::stop_workers() {
    #ifdef DISCUSY_LOGGING
    log("Stopping workers");
    #endif
    if (work_guard) {
        work_guard->reset();
        work_guard.reset();
    }

    io_ctx->stop(); // stops all the timers
}

void bot::route_evacuated_payload(std::string&& payload) {
    const auto total_shards = total_shards_.load();
    if (total_shards == 0) {
        const std::scoped_lock q_lock{pending_shard_operations_mtx_};
        pending_shard_operations_queue_.emplace_back([this, p = std::move(payload)] mutable {
            this->route_evacuated_payload(std::move(p));
        });
        return;
    }

    send_event::rescue_payload obj{};
    if (gateway_protocol::parse(obj, payload, json_logger)) {
        return;
    }

    if (!obj.d.is_object()) {
        #ifdef DISCUSY_LOGGING
        log("Object in route_evacuated_payload isn't object??\n{}", payload);
        #endif
        return;
    }

    auto& d = obj.d.get_object();
    const auto *const d_end = d.end();

    auto *const g_id_it = d.find("guild_id");
    uint64_t g_id_val = 0;
    if (g_id_it != d_end) {
        if (g_id_it->second.is_string()) {
            g_id_val = ulp::str::to_number_or_default<decltype(snowflake::value)>(g_id_it->second.get_string());
        } else if (g_id_it->second.holds<uint64_t>()) {
            g_id_val = g_id_it->second.get<uint64_t>();
        } else if (g_id_it->second.holds<int64_t>()) {
            const auto v = g_id_it->second.get<int64_t>();
            if (v > 0) g_id_val = static_cast<uint64_t>(v);
        } else if (g_id_it->second.is_number()) {
            g_id_val = static_cast<decltype(snowflake::value)>(g_id_it->second.get_number());
        }
    }
    const snowflake guild_id{g_id_val};

    auto *g_ids_it = d.find("guild_ids");
    std::vector<snowflake> guild_ids;
    if (g_ids_it != d_end && g_ids_it->second.is_array()) {
        for (const auto& j : g_ids_it->second.get_array()) {
            if (j.is_string()) {
                const auto num = ulp::str::to_number<decltype(snowflake::value)>(j.get_string());
                if (!num || *num == 0ULL) continue;
                guild_ids.emplace_back(*num);
            } else if (j.holds<uint64_t>()) {
                const auto num = j.get<uint64_t>();
                if (num == 0ULL) continue;
                guild_ids.emplace_back(num);
            } else if (j.holds<int64_t>()) {
                const auto num = j.get<int64_t>();
                if (num <= 0) continue;
                guild_ids.emplace_back(static_cast<uint64_t>(num));
            } else if (j.is_number()) {
                const auto num = static_cast<decltype(snowflake::value)>(j.get_number());
                if (num == 0ULL) continue;
                guild_ids.emplace_back(num);
            }
        }

        g_ids_it->second.get_array().clear();
    }

    if ((!guild_id && guild_ids.empty()) || (guild_id && !guild_ids.empty())) {
        // how?
        #ifdef DISCUSY_LOGGING
        log("Object in route_evacuated_payload has no guild id or guild ids, or has both??\n{}", payload);
        #endif
        return;
    }

    const std::scoped_lock lock{shards_mutex};
    if (shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
        const std::scoped_lock q_lock{pending_shard_operations_mtx_};
        pending_shard_operations_queue_.emplace_back([this, p = std::move(payload)] mutable {
            this->route_evacuated_payload(std::move(p));
        });
        return;
    }

    if (guild_id) {
        const auto new_shard_id = guild_id.guild_shard_id(total_shards);
        if (new_shard_id < shards_.size()) {
            shards_[new_shard_id]->send_text_ratelimited(std::move(payload));
        }
    } else { // guild_ids
        std::vector<send_event::rescue_payload> shard_requests(total_shards);
        for (const auto& id : guild_ids) {
            const auto shard_id = id.guild_shard_id(total_shards);
            if (shard_id >= total_shards) continue;

            if (!shard_requests[shard_id].d.is_object()) {
                shard_requests[shard_id].op = obj.op;
                shard_requests[shard_id].d = d;
            }

            // these will all exist
            #ifdef DISCUSY_USE_ETF
            shard_requests[shard_id].d.at("guild_ids").get_array().emplace_back(id.value);
            #else
            shard_requests[shard_id].d.at("guild_ids").get_array().emplace_back(id.str());
            #endif
        }

        std::string encoded;

        for (std::decay_t<decltype(total_shards)> i = 0; i < total_shards; ++i) {
            auto& request = shard_requests[i];
            if (!request.d.is_object()) continue;
            auto& g_ids = request.d.at("guild_ids");
            if (!g_ids.is_array() || g_ids.get_array().empty()) continue;

            encoded.clear();
            if (gateway_protocol::write(request, encoded)) continue;

            if (i < shards_.size()) {
                shards_[i]->send_text_ratelimited(std::move(encoded));
            }
        }
    }
}

} // namespace discusy
