#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include <boost/asio.hpp>

#include <sodium.h>
#include <boost/container/deque.hpp>

#include <glaze/util/inline.hpp>

#include "../types.hpp"
#include "../audio_runtime.hpp"
#include "../io_context.hpp"
#include "../log.hpp" // IWYU pragma: keep
#include "internal.hpp"

namespace discusy::udp {

using audio_options = boost::container::deque_options<
    boost::container::block_bytes<65536>
>::type;

template <std::integral T>
[[nodiscard]] GLZ_ALWAYS_INLINE constexpr T to_network_order(T value) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(value);
    } else if constexpr (std::endian::native == std::endian::big) {
        return value;
    } else {
        static_assert(std::endian::native == std::endian::little ||
                      std::endian::native == std::endian::big,
                      "Mixed-endian architectures are not supported!");
    }
}

template <std::integral T>
[[nodiscard]] GLZ_ALWAYS_INLINE constexpr T from_network_order(T value) noexcept {
    return to_network_order(value);
}

enum class encryption_mode : std::uint8_t {
    aead_aes256_gcm_rtpsize,
    aead_xchacha20_poly1305_rtpsize,
};

using boost::asio::ip::udp;

template <typename errCB, typename encryptCB, typename idleCB, typename receiveCB>
requires (
    std::invocable<std::decay_t<errCB>, boost::system::error_code> &&
    std::invocable<std::decay_t<encryptCB>, const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>&, discusy::voice::store<discusy::voice::MAX_DAVE_BUFFER_SIZE>&> &&
    std::invocable<std::decay_t<idleCB>, discusy::voice::audio_stop_reason> &&
    std::invocable<std::decay_t<receiveCB>, std::uint32_t, std::uint16_t, std::uint32_t, std::span<const std::uint8_t>>
)
class client : public std::enable_shared_from_this<client<errCB, encryptCB, idleCB, receiveCB>> {
public:

    template <typename F, typename F2, typename F3, typename F4>
    requires (
        std::invocable<F&, boost::system::error_code> &&
        std::invocable<F2&, const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>&, discusy::voice::store<discusy::voice::MAX_DAVE_BUFFER_SIZE>&> &&
        std::invocable<F3&, discusy::voice::audio_stop_reason> &&
        std::invocable<F4&, std::uint32_t, std::uint16_t, std::uint32_t, std::span<const std::uint8_t>>
    )
    client(ctx::io_context& io_context, F&& cb, F2&& cb2, F3&& cb3, F4&& cb4)
        : on_error_{std::forward<F>(cb)}, encrypt_cb_{std::forward<F2>(cb2)}, on_idle_{std::forward<F3>(cb3)}, receive_cb_{std::forward<F4>(cb4)},
        io_ctx_{io_context}, strand_{make_pacing_strand(io_context)}, socket_{strand_, udp::endpoint(udp::v4(), 0)},
        resolver_{strand_}, timer_{strand_}, silence_frame_{discusy::voice::opus_silence}
    {
        boost::system::error_code ec;
        static const bool sodium_initialized = (sodium_init() >= 0);
        (void)sodium_initialized;
        socket_.non_blocking(true, ec);
        apply_socket_options();
        socket_broken_ = ec || !socket_.is_open();
    }

    void apply_socket_options() {
        boost::system::error_code ec;
        socket_.set_option(boost::asio::socket_base::send_buffer_size(64 * 1024), ec);
        socket_.set_option(boost::asio::socket_base::receive_buffer_size(64 * 1024), ec);

        #ifdef _WIN32
        #ifndef SIO_UDP_CONNRESET
        #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
        #endif
        BOOL new_behavior = FALSE;
        DWORD bytes_returned = 0;
        ::WSAIoctl(socket_.native_handle(), SIO_UDP_CONNRESET, &new_behavior, sizeof(new_behavior), nullptr, 0, &bytes_returned, nullptr, nullptr);
        #endif

        #ifdef IP_TOS
        int tos = 0xB8;
        ::setsockopt(socket_.native_handle(), IPPROTO_IP, IP_TOS, reinterpret_cast<const char*>(&tos), sizeof(tos));
        #endif
    }

    void reset_session() {
        boost::asio::post(strand_, [s = this->shared_from_this()] {
            auto& self = *s;
            if (self.closed_) return;

            self.begin_new_session();
        });
    }

    template <typename F>
    requires ( std::invocable<F&, std::string, std::uint16_t> )
    void send_ip_discovery(std::string host, std::uint16_t port, std::uint32_t ssrc, F&& cb) {
        boost::asio::post(strand_, [s = this->shared_from_this(), host = std::move(host), port, ssrc, cb = std::forward<F>(cb)] mutable {
            auto& self = *s;
            if (self.closed_) return std::invoke(std::move(cb), std::string{}, std::uint16_t{0});

            self.do_ip_discovery(host, port, ssrc, std::move(cb));
        });
    }

    void set_secret_key(const encryption_mode mode, const std::span<const uint8_t, 32> secret_key) {
        std::array<std::uint8_t, 32> key{};
        std::memcpy(key.data(), secret_key.data(), key.size());

        boost::asio::post(strand_, [s = this->shared_from_this(), mode, key] {
            auto& self = *s;
            if (self.closed_) return;

            self.mode_ = mode;
            self.secret_key_ = key;
            self.keyed_ = true;

            self.drain();
        });
    }

    void kick_drain() {
        boost::asio::post(strand_, [s = this->shared_from_this()] {
            if (s->closed_) return;
            s->drain();
        });
    }

    bool send_audio_packet(const std::span<const std::uint8_t> opus_frame) {
        if (opus_frame.size() > discusy::voice::MAX_OPUS_FRAME_BYTES) return false;
        if (closed_.load(std::memory_order_acquire)) return false;

        try {
            boost::asio::dispatch(strand_, [s = this->shared_from_this(), frame = discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>{opus_frame}] mutable {
                auto& self = *s;
                if (self.closed_) return;

                try {
                    self.audio_packets_.emplace_back(std::move(frame));
                    self.queued_audio_frames_.store(self.queued_audio_frames_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
                    self.next_idle_reason_ = discusy::voice::audio_stop_reason::finished;
                    self.drain();
                }
                catch (...) { // NOLINT(bugprone-empty-catch)
                    #ifdef DISCUSY_LOGGING
                    self.log("send_audio_packet dispatch unknown exception");
                    #endif
                }
            });
            return true;
        }
        catch (...) {
            return false;
        }
    }

    bool send_audio_packets(std::vector<discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>>&& frames) {
        if (frames.empty()) return true;
        if (closed_.load(std::memory_order_acquire)) return false;

        try {
            boost::asio::dispatch(strand_, [s = this->shared_from_this(), frames = std::move(frames)] mutable {
                auto& self = *s;
                if (self.closed_) return;

                try {
                    const auto count = frames.size();
                    for (auto& f : frames) {
                        self.audio_packets_.emplace_back(std::move(f));
                    }
                    self.queued_audio_frames_.store(self.queued_audio_frames_.load(std::memory_order_relaxed) + count, std::memory_order_relaxed);
                    self.next_idle_reason_ = discusy::voice::audio_stop_reason::finished;
                    self.drain();
                }
                catch (...) { // NOLINT(bugprone-empty-catch)
                    #ifdef DISCUSY_LOGGING
                    self.log("send_audio_packets dispatch unknown exception");
                    #endif
                }
            });
            return true;
        }
        catch (...) {
            return false;
        }
    }

    bool send_audio_packets(std::span<const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>> frames) {
        if (frames.empty()) return true;
        if (closed_.load(std::memory_order_acquire)) return false;

        try {
            std::vector<discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>> frames_copy(frames.begin(), frames.end());
            return send_audio_packets(std::move(frames_copy));
        }
        catch (...) {
            return false;
        }
    }

    void close() {
        boost::asio::post(strand_, [self = this->shared_from_this()] {
            if (self->closed_.load(std::memory_order_relaxed)) return;
            self->closed_.store(true, std::memory_order_release);
            self->armed_ = false;
            self->keyed_ = false;
            self->draining_ = false;
            self->drain_pending_ = false;
            self->has_prepared_packet_ = false;
            ++self->session_;
            ++self->timer_gen_;

            boost::system::error_code ec;
            self->socket_.cancel(ec);
            self->socket_.close(ec);
            self->resolver_.cancel();
            self->timer_.cancel();
            self->audio_packets_.clear();
            self->queued_audio_frames_.store(0, std::memory_order_relaxed);
        });
    }

    void stop_audio() {
        boost::asio::dispatch(strand_, [s = this->shared_from_this()] mutable {
            auto& self = *s;
            if (self.closed_) return;

            self.audio_packets_.clear();
            self.queued_audio_frames_.store(0, std::memory_order_relaxed);
            self.paused_ = false;
            self.pause_frames_remaining_ = 0;
            self.next_idle_reason_ = discusy::voice::audio_stop_reason::stopped;
            self.underrun_ticks_ = 0;
            self.idle_ = false;
            self.drain();
        });
    }

    void pause_audio() {
        boost::asio::dispatch(strand_, [s = this->shared_from_this()] mutable {
            auto& self = *s;
            if (self.closed_ || self.paused_) return;

            self.paused_ = true;
            self.pause_frames_remaining_ = self.underrun_grace_frames_.load(std::memory_order_relaxed);
            self.next_idle_reason_ = discusy::voice::audio_stop_reason::paused;
            self.idle_ = false;

            self.drain();
        });
    }

    void resume_audio() {
        boost::asio::dispatch(strand_, [s = this->shared_from_this()] mutable {
            auto& self = *s;
            if (self.closed_) return;

            self.paused_ = false;
            self.pause_frames_remaining_ = 0;
            self.underrun_ticks_ = 0;
            self.next_idle_reason_ = discusy::voice::audio_stop_reason::finished;
            self.drain();
        });
    }

    [[nodiscard]] std::uint64_t get_frames_remaining() const noexcept {
        return queued_audio_frames_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::chrono::milliseconds get_ms_remaining() const noexcept {
        return std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(get_frames_remaining()) * discusy::voice::DISCORD_FRAME_SIZE_MS};
    }

    [[nodiscard]] double get_secs_remaining() const noexcept {
        return static_cast<double>(get_frames_remaining() * discusy::voice::DISCORD_FRAME_SIZE_MS) / 1000.0;
    }

    void set_underrun_grace_frames(const std::uint32_t frames) noexcept {
        underrun_grace_frames_.store(std::max<std::uint32_t>(1, frames), std::memory_order_relaxed);
    }

    void set_max_catchup_frames(const std::uint32_t frames) noexcept {
        max_catchup_frames_.store(std::max<std::uint32_t>(1, frames), std::memory_order_relaxed);
    }

    void set_not_ready_hold_frames(const std::uint32_t frames) noexcept {
        not_ready_hold_frames_.store(frames, std::memory_order_relaxed);
    }

    [[nodiscard]] discusy::voice::audio_stats get_stats() const noexcept {
        return discusy::voice::audio_stats{
            .frames_sent         = stat_frames_sent_.load(std::memory_order_relaxed),
            .silence_frames_sent = stat_silence_frames_.load(std::memory_order_relaxed),
            .underrun_events     = stat_underrun_events_.load(std::memory_order_relaxed),
            .underrun_frames     = stat_underrun_frames_.load(std::memory_order_relaxed),
            .clock_resyncs       = stat_clock_resyncs_.load(std::memory_order_relaxed),
            .frames_skipped      = stat_frames_skipped_.load(std::memory_order_relaxed),
            .send_errors         = stat_send_errors_.load(std::memory_order_relaxed),
            .would_block_drops   = stat_would_block_drops_.load(std::memory_order_relaxed),
            .encrypt_drops       = stat_encrypt_drops_.load(std::memory_order_relaxed),
            .not_ready_stalls    = stat_not_ready_stalls_.load(std::memory_order_relaxed),
            .no_key_drops        = stat_no_key_drops_.load(std::memory_order_relaxed),
            .worst_lateness_us   = stat_worst_lateness_us_.load(std::memory_order_relaxed),
            .packets_received    = stat_packets_received_.load(std::memory_order_relaxed),
            .receive_decrypt_drops = stat_receive_decrypt_drops_.load(std::memory_order_relaxed),
        };
    }

    void reset_stats() noexcept {
        stat_frames_sent_.store(0, std::memory_order_relaxed);
        stat_silence_frames_.store(0, std::memory_order_relaxed);
        stat_underrun_events_.store(0, std::memory_order_relaxed);
        stat_underrun_frames_.store(0, std::memory_order_relaxed);
        stat_clock_resyncs_.store(0, std::memory_order_relaxed);
        stat_frames_skipped_.store(0, std::memory_order_relaxed);
        stat_send_errors_.store(0, std::memory_order_relaxed);
        stat_would_block_drops_.store(0, std::memory_order_relaxed);
        stat_encrypt_drops_.store(0, std::memory_order_relaxed);
        stat_not_ready_stalls_.store(0, std::memory_order_relaxed);
        stat_no_key_drops_.store(0, std::memory_order_relaxed);
        stat_worst_lateness_us_.store(0, std::memory_order_relaxed);
        stat_packets_received_.store(0, std::memory_order_relaxed);
        stat_receive_decrypt_drops_.store(0, std::memory_order_relaxed);
    }

    using error_cb_t = std::decay_t<errCB>;
    using encrypt_cb_t = std::decay_t<encryptCB>;
    using idle_cb_t = std::decay_t<idleCB>;
    using receive_cb_t = std::decay_t<receiveCB>;

private:
    using clock = std::chrono::steady_clock;

    static constexpr std::uint32_t MAX_CONSECUTIVE_SEND_ERRORS = 50;
    static constexpr std::uint32_t MAX_CONSECUTIVE_RECV_ERRORS = 20;

    static ctx::io_context::strand_t make_pacing_strand([[maybe_unused]] ctx::io_context& io_context) {
        #ifdef DISCUSY_NO_DEDICATED_AUDIO_THREAD
        return io_context.make_strand();
        #else
        return ctx::audio_runtime::instance().make_strand();
        #endif
    }

    error_cb_t on_error_;
    encrypt_cb_t encrypt_cb_;
    idle_cb_t on_idle_;
    receive_cb_t receive_cb_;

    #ifdef DISCUSY_LOGGING
    template <typename ...Args>
    void log(const std::format_string<Args...>& s, Args&&... args) {
        log::Logger{}("[discusy udp client] {}", std::format(s, std::forward<Args>(args)...));
    }
    #endif

    [[nodiscard]] bool can_transmit() const noexcept {
        return !closed_ && armed_ && keyed_;
    }

    void begin_new_session() {
        ++session_;
        ++timer_gen_;

        armed_ = false;
        keyed_ = false;
        failed_ = false;
        draining_ = false;
        drain_pending_ = false;
        has_prepared_packet_ = false;

        boost::system::error_code ec;
        socket_.cancel(ec);
        resolver_.cancel();
        timer_.cancel();

        boost::system::error_code ignored;
        socket_.close(ignored);

        socket_.open(udp::v4(), ec);
        if (!ec) socket_.bind(udp::endpoint(udp::v4(), 0), ec);
        if (!ec) socket_.non_blocking(true, ec);
        if (!ec) apply_socket_options();

        if (ec) {
            socket_.close(ignored);

            #ifdef DISCUSY_LOGGING
            log("Failed to reopen the udp socket: {}", ec.message());
            #endif
        }

        socket_broken_ = !socket_.is_open();

        sequence_ = 0;
        timestamp_ = 0;
        nonce_counter_ = 0;
        secret_key_.fill(0);

        underrun_ticks_ = 0;
        consecutive_send_errors_ = 0;
        consecutive_recv_errors_ = 0;
    }

    template <typename F>
    void do_ip_discovery(const std::string& host, std::uint16_t port, std::uint32_t ssrc, F&& cb) {
        if (socket_broken_ || !socket_.is_open()) {
            #ifdef DISCUSY_LOGGING
            log("Skipping IP discovery: the udp socket could not be opened");
            #endif
            return std::invoke(std::forward<F>(cb), std::string{}, std::uint16_t{0});
        }

        ssrc_ = ssrc;
        ssrc_net_order_ = to_network_order(ssrc);

        #ifdef DISCUSY_LOGGING
        log("Starting udp client to host and port: {}, {} (ssrc {})", host, port, ssrc);
        #endif

        discovery_packet_.fill(0);

        // request
        discovery_packet_[0] = 0x00;
        discovery_packet_[1] = 0x01;
        // length 70
        discovery_packet_[2] = 0x00;
        discovery_packet_[3] = 0x46;
        // ssrc
        discovery_packet_[4] = static_cast<std::uint8_t>((ssrc >> 24U) & 0xFFU);
        discovery_packet_[5] = static_cast<std::uint8_t>((ssrc >> 16U) & 0xFFU);
        discovery_packet_[6] = static_cast<std::uint8_t>((ssrc >> 8U) & 0xFFU);
        discovery_packet_[7] = static_cast<std::uint8_t>(ssrc & 0xFFU);

        resolver_.async_resolve(udp::v4(), host, discusy::number_str{port}.view(),
            boost::asio::cancel_after(std::chrono::seconds{5},
                [s = this->shared_from_this(), session = session_, cb = std::forward<F>(cb)](const boost::system::error_code& error, const udp::resolver::results_type& results) mutable {
                    auto& self = *s;
                    if (self.closed_ || (self.session_ != session)) return;

                    if (error || results.empty()) {
                        #ifdef DISCUSY_LOGGING
                        self.log("Error during async resolve: {} ({})", error.message(), error.value());
                        #endif
                        return std::invoke(std::move(cb), std::string{}, std::uint16_t{0});
                    }

                    self.remote_endpoint_ = *results.begin();
                    #ifdef DISCUSY_LOGGING
                    self.log("Successfully resolved endpoint.");
                    #endif

                    self.socket_.async_send_to(
                        boost::asio::buffer(self.discovery_packet_), self.remote_endpoint_,
                        boost::asio::cancel_after(std::chrono::seconds{15},
                            [s, session, cb = std::move(cb)](const boost::system::error_code& error, std::size_t /* bytes_transferred */) mutable {
                                auto& self = *s;
                                if (self.closed_ || (self.session_ != session)) return;

                                if (error) {
                                    #ifdef DISCUSY_LOGGING
                                    self.log("Error sending IP discovery: {}", error.message());
                                    #endif
                                    return std::invoke(std::move(cb), std::string{}, std::uint16_t{0});
                                }

                                self.discovery_packet_.fill(0);
                                self.socket_.async_receive_from(
                                    boost::asio::buffer(self.discovery_packet_), self.discovery_sender_,
                                    boost::asio::cancel_after(std::chrono::seconds{15},
                                        [s, session, cb = std::move(cb)](const boost::system::error_code& error, std::size_t bytes_transferred) mutable {
                                            auto& self = *s;
                                            if (self.closed_ || (self.session_ != session)) return;

                                            #ifdef DISCUSY_LOGGING
                                            if (error) {
                                                self.log("Error during IP discovery receive: {} ({})", error.message(), error.value());
                                            } else {
                                                self.log("Received data from IP discovery: size {}", bytes_transferred);
                                            }
                                            #endif

                                            if (error || (bytes_transferred != 74) || (self.discovery_sender_ != self.remote_endpoint_)) {
                                                return std::invoke(std::move(cb), std::string{}, std::uint16_t{0});
                                            }

                                            auto& buf = self.discovery_packet_;
                                            if (buf[0] != 0x00 || buf[1] != 0x02 || buf[2] != 0x00 || buf[3] != 0x46) {
                                                return std::invoke(std::move(cb), std::string{}, std::uint16_t{0});
                                            }

                                            const char* addr = reinterpret_cast<const char*>(&buf[8]);
                                            const char* addr_end = std::find(addr, addr + 64, '\0');

                                            std::string my_ip{addr, addr_end};
                                            const auto my_port = static_cast<std::uint16_t>((static_cast<unsigned>(buf[72]) << 8U) | static_cast<unsigned>(buf[73]));

                                            boost::system::error_code conn_ec;
                                            self.socket_.connect(self.remote_endpoint_, conn_ec);
                                            if (conn_ec) {
                                                #ifdef DISCUSY_LOGGING
                                                self.log("Failed to connect udp socket: {}", conn_ec.message());
                                                #endif
                                                return std::invoke(std::move(cb), std::string{}, std::uint16_t{0});
                                            }

                                            self.armed_ = true;
                                            self.start_receive();
                                            self.drain();

                                            std::invoke(std::move(cb), std::move(my_ip), my_port);
                                        })
                                );
                            })
                    );
                }));
    }

    bool encrypt_voice_packet(const std::span<const uint8_t, 12> rtp_header, const std::span<const uint8_t> frame_data) {
        static constexpr std::size_t AEAD_OVERHEAD = 16;
        if (rtp_header.size() + frame_data.size() + AEAD_OVERHEAD + 4 > discusy::voice::MAX_FINAL_BUFFER_SIZE) {
            return false;
        }

        const std::uint32_t big_endian_counter = to_network_order(nonce_counter_++);

        unsigned long long ciphertext_len = 0;
        uint8_t* ciphertext_dest = next_buf_.raw_buf.data() + rtp_header.size();

        std::memcpy(next_buf_.raw_buf.data(), rtp_header.data(), rtp_header.size());

        if (mode_ == encryption_mode::aead_xchacha20_poly1305_rtpsize) {
            std::array<std::uint8_t, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> sodium_nonce{};
            std::memcpy(sodium_nonce.data(), &big_endian_counter, sizeof(big_endian_counter));

            if (crypto_aead_xchacha20poly1305_ietf_encrypt(
                ciphertext_dest, &ciphertext_len,
                frame_data.data(), frame_data.size(),
                rtp_header.data(), rtp_header.size(),
                nullptr, sodium_nonce.data(), secret_key_.data()
            ) != 0) return false;
        }
        else if (mode_ == encryption_mode::aead_aes256_gcm_rtpsize) {
            std::array<std::uint8_t, crypto_aead_aes256gcm_NPUBBYTES> sodium_nonce{};
            std::memcpy(sodium_nonce.data(), &big_endian_counter, sizeof(big_endian_counter));

            if (crypto_aead_aes256gcm_encrypt(
                ciphertext_dest, &ciphertext_len,
                frame_data.data(), frame_data.size(),
                rtp_header.data(), rtp_header.size(),
                nullptr, sodium_nonce.data(), secret_key_.data()
            ) != 0) return false;
        } else {
            return false;
        }

        std::memcpy(ciphertext_dest + ciphertext_len, &big_endian_counter, 4);
        next_buf_.set_size(rtp_header.size() + ciphertext_len + 4);

        return true;
    }

    void process_received_packet(const std::size_t bytes_transferred) {
        static constexpr std::size_t AEAD_OVERHEAD = 16;
        static constexpr std::uint8_t DISCORD_OPUS_PAYLOAD_TYPE = 0x78U;
        if (!keyed_ || !armed_ || bytes_transferred < 12 + AEAD_OVERHEAD + 4) return;
        if ((static_cast<unsigned>(recv_buffer_[0]) >> 6U) != 2U) return; // RTP v2 only
        if (recv_buffer_[1] >= 192U && recv_buffer_[1] <= 223U) return; // Ignore RTCP (RFC 5761)
        if ((recv_buffer_[1] & 0x7FU) != DISCORD_OPUS_PAYLOAD_TYPE) return; // Opus audio only

        const auto cc = static_cast<std::size_t>(recv_buffer_[0] & 0x0FU);
        const bool has_ext = (recv_buffer_[0] & 0x10U) != 0U;
        const bool has_padding = (recv_buffer_[0] & 0x20U) != 0U;

        const std::size_t base_header_len = 12 + (cc * 4);
        const std::size_t aad_len = base_header_len + (has_ext ? 4 : 0);
        if (bytes_transferred < aad_len + AEAD_OVERHEAD + 4) return;

        std::uint16_t seq{};
        std::memcpy(&seq, &recv_buffer_[2], sizeof(seq));
        seq = from_network_order(seq);

        std::uint32_t ts{};
        std::memcpy(&ts, &recv_buffer_[4], sizeof(ts));
        ts = from_network_order(ts);

        std::uint32_t ssrc{};
        std::memcpy(&ssrc, &recv_buffer_[8], sizeof(ssrc));
        ssrc = from_network_order(ssrc);

        const std::size_t ciphertext_len = bytes_transferred - aad_len - 4;
        const std::uint8_t* ciphertext = recv_buffer_.data() + aad_len;
        const std::uint8_t* nonce_suffix = recv_buffer_.data() + (bytes_transferred - 4);

        unsigned long long plaintext_len = 0;
        int decrypt_rc = -1;

        if (mode_ == encryption_mode::aead_xchacha20_poly1305_rtpsize) {
            std::array<std::uint8_t, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> sodium_nonce{};
            std::memcpy(sodium_nonce.data(), nonce_suffix, 4);

            decrypt_rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
                recv_decrypt_buf_.data(), &plaintext_len,
                nullptr,
                ciphertext, ciphertext_len,
                recv_buffer_.data(), aad_len,
                sodium_nonce.data(), secret_key_.data()
            );
        } else if (mode_ == encryption_mode::aead_aes256_gcm_rtpsize) {
            std::array<std::uint8_t, crypto_aead_aes256gcm_NPUBBYTES> sodium_nonce{};
            std::memcpy(sodium_nonce.data(), nonce_suffix, 4);

            decrypt_rc = crypto_aead_aes256gcm_decrypt(
                recv_decrypt_buf_.data(), &plaintext_len,
                nullptr,
                ciphertext, ciphertext_len,
                recv_buffer_.data(), aad_len,
                sodium_nonce.data(), secret_key_.data()
            );
        }

        if (decrypt_rc != 0) {
            stat_receive_decrypt_drops_.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        std::size_t payload_offset = 0;
        auto payload_len = static_cast<std::size_t>(plaintext_len);

        if (has_ext) {
            std::uint16_t ext_words{};
            std::memcpy(&ext_words, &recv_buffer_[base_header_len + 2], sizeof(ext_words));
            ext_words = from_network_order(ext_words);

            const std::size_t ext_bytes = static_cast<std::size_t>(ext_words) * 4;
            if (ext_bytes > payload_len) return;
            payload_offset += ext_bytes;
            payload_len -= ext_bytes;
        }

        if (has_padding && payload_len > 0) {
            const std::size_t pad_len = recv_decrypt_buf_[payload_offset + payload_len - 1];
            if (pad_len == 0 || pad_len > payload_len) return;
            payload_len -= pad_len;
        }

        if (payload_len == 0) return;

        stat_packets_received_.fetch_add(1, std::memory_order_relaxed);
        std::invoke(
            receive_cb_,
            ssrc,
            seq,
            ts,
            std::span<const std::uint8_t>{recv_decrypt_buf_.data() + payload_offset, payload_len}
        );
    }

    void start_receive() {
        if (closed_ || !armed_) return;

        socket_.async_receive(
            boost::asio::buffer(recv_buffer_),
            [self = this->shared_from_this(), session = session_](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (self->closed_ || (self->session_ != session)) return;
                self->handle_receive(error, bytes_transferred);
            }
        );
    }

    void handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (closed_) return;

        if (error == boost::asio::error::operation_aborted) return; // whoever cancelled owns it

        if (!error) {
            consecutive_recv_errors_ = 0;
            process_received_packet(bytes_transferred);
            start_receive();
            return;
        }

        #ifdef DISCUSY_LOGGING
        log("UDP receive error (continuing): {}", error.message());
        #endif

        if (++consecutive_recv_errors_ > MAX_CONSECUTIVE_RECV_ERRORS) {
            #ifdef DISCUSY_LOGGING
            log("UDP receive failing persistently, reporting it");
            #endif
            consecutive_recv_errors_ = 0;
            return fail(error);
        }

        start_receive();
    }

    void fail(const boost::system::error_code& ec) {
        if (closed_ || failed_) return;

        failed_ = true;
        armed_ = false;
        keyed_ = false;
        has_prepared_packet_ = false;

        std::invoke(on_error_, ec);
    }

    void drain() {
        if (closed_) return;

        if (draining_) {
            drain_pending_ = true;
            return;
        }

        if (!can_transmit()) return;
        if (paused_ && (pause_frames_remaining_ == 0)) return;
        if (audio_packets_.empty() && idle_ && !paused_) return;

        draining_ = true;
        drain_pending_ = false;
        idle_ = false;
        underrun_ticks_ = 0;
        not_ready_retries_ = 0;
        has_prepared_packet_ = false;
        next_frame_tp_ = clock::now();

        // rtp_header_.fill(0);
        rtp_header_[0] = 0x80;
        rtp_header_[1] = 0x78;
        std::memcpy(&rtp_header_[8], &ssrc_net_order_, 4);

        if (!prepare_next_packet()) {
            if (draining_) {
                park_drain();
            }
            return;
        }

        arm_timer();
    }

    void park_drain() {
        draining_ = false;
        if (std::exchange(drain_pending_, false)) drain();
    }

    void finish_drain(const discusy::voice::audio_stop_reason reason) {
        draining_ = false;
        idle_ = true;
        underrun_ticks_ = 0;
        has_prepared_packet_ = false;

        if (reason != discusy::voice::audio_stop_reason::paused) {
            paused_ = false;
            pause_frames_remaining_ = 0;
        }

        std::invoke(on_idle_, reason);

        if (std::exchange(drain_pending_, false)) drain();
    }

    void advance_clock() {
        next_frame_tp_ += discusy::voice::DISCORD_FRAME_DURATION;

        const auto now = clock::now();
        if (now <= next_frame_tp_) return;

        const auto late = now - next_frame_tp_;

        const auto late_us = std::chrono::duration_cast<std::chrono::microseconds>(late).count();
        if (late_us > stat_worst_lateness_us_.load(std::memory_order_relaxed)) {
            stat_worst_lateness_us_.store(late_us, std::memory_order_relaxed);
        }

        const auto allowance = discusy::voice::DISCORD_FRAME_DURATION * max_catchup_frames_.load(std::memory_order_relaxed);
        if (late <= allowance) return;

        const auto skipped = static_cast<std::uint32_t>((late - allowance) / discusy::voice::DISCORD_FRAME_DURATION) + 1;

        next_frame_tp_ += discusy::voice::DISCORD_FRAME_DURATION * skipped;
        timestamp_ += skipped * discusy::voice::TIMESTAMP_INCREMENT;

        stat_clock_resyncs_.fetch_add(1, std::memory_order_relaxed);
        stat_frames_skipped_.fetch_add(skipped, std::memory_order_relaxed);

        #ifdef DISCUSY_LOGGING
        log("Pacing fell {}ms behind, skipping {} frames of media time",
            std::chrono::duration_cast<std::chrono::milliseconds>(late).count(), skipped);
        #endif
    }

    bool transmit_current_packet() {
        boost::system::error_code ec;
        socket_.send(boost::asio::buffer(next_buf_.span()), 0, ec);

        if (!ec) {
            consecutive_send_errors_ = 0;
            stat_frames_sent_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
            stat_would_block_drops_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        if (ec == boost::asio::error::operation_aborted) return false;

        stat_send_errors_.fetch_add(1, std::memory_order_relaxed);

        #ifdef DISCUSY_LOGGING
        log("Failed to send RTP packet: {}", ec.message());
        #endif

        if (++consecutive_send_errors_ > MAX_CONSECUTIVE_SEND_ERRORS) {
            #ifdef DISCUSY_LOGGING
            log("UDP sending failing persistently, reporting it");
            #endif
            consecutive_send_errors_ = 0;
            fail(ec);
            return false;
        }

        return true;
    }

    void arm_timer() {
        const auto gen = ++timer_gen_;
        timer_.expires_at(next_frame_tp_);
        timer_.async_wait([s = this->shared_from_this(), gen](const boost::system::error_code& ec) {
            if (s->closed_.load(std::memory_order_relaxed) || (s->timer_gen_ != gen)) return;
            if (ec) {
                s->park_drain();
                return;
            }
            s->tick();
        });
    }

    void tick() {
        try {
            tick_impl();
        }
        #ifdef DISCUSY_LOGGING
        catch (const std::exception& e) {
            log("Exception in the pacing tick: {}, stopping this drain", e.what());
            draining_ = false;
            drain_pending_ = false;
            has_prepared_packet_ = false;
        }
        #endif
        catch (...) {
            #ifdef DISCUSY_LOGGING
            log("Unknown exception in the pacing tick, stopping this drain");
            #endif
            draining_ = false;
            drain_pending_ = false;
            has_prepared_packet_ = false;
        }
    }

    bool prepare_next_packet() {
        if (closed_.load(std::memory_order_relaxed) || !can_transmit()) {
            has_prepared_packet_ = false;
            return false;
        }

        enum class frame_source : std::uint8_t {
            pause_silence,
            queued_audio,
            underrun_silence,
        };

        const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>* frame = nullptr;
        frame_source source = frame_source::underrun_silence;

        if (paused_) {
            if (pause_frames_remaining_ == 0) {
                finish_drain(discusy::voice::audio_stop_reason::paused);
                return false;
            }
            frame = &silence_frame_;
            source = frame_source::pause_silence;
        } else if (!audio_packets_.empty()) {
            frame = &audio_packets_.front();
            source = frame_source::queued_audio;
        } else {
            if (underrun_ticks_ >= underrun_grace_frames_.load(std::memory_order_relaxed)) {
                finish_drain(next_idle_reason_);
                return false;
            }
            frame = &silence_frame_;
            source = frame_source::underrun_silence;
        }

        dave_buf_.clear();
        const auto encrypted = encrypt_cb_(*frame, dave_buf_);
        bool no_key_drop = false;

        if (encrypted == discusy::voice::encrypt_result::not_ready) {
            if (not_ready_retries_ < std::numeric_limits<std::uint32_t>::max()) ++not_ready_retries_;

            if (not_ready_retries_ <= not_ready_hold_frames_.load(std::memory_order_relaxed)) {
                stat_not_ready_stalls_.fetch_add(1, std::memory_order_relaxed);
                has_prepared_packet_ = false;
                return true;
            }

            #ifdef DISCUSY_LOGGING
            if (not_ready_retries_ == (not_ready_hold_frames_.load(std::memory_order_relaxed) + 1)) {
                log("No encryption key after {} frames, dropping audio until one arrives", not_ready_retries_ - 1);
            }
            #endif

            no_key_drop = true;
        } else {
            #ifdef DISCUSY_LOGGING
            if (not_ready_retries_ > not_ready_hold_frames_.load(std::memory_order_relaxed)) {
                log("Encryption key available again after {} frames", not_ready_retries_);
            }
            #endif
            not_ready_retries_ = 0;
        }

        if (source == frame_source::queued_audio) {
            if (underrun_ticks_ > 0) {
                stat_underrun_events_.fetch_add(1, std::memory_order_relaxed);
                stat_underrun_frames_.fetch_add(underrun_ticks_, std::memory_order_relaxed);
                underrun_ticks_ = 0;
            }
            audio_packets_.pop_front();
            const auto current = queued_audio_frames_.load(std::memory_order_relaxed);
            if (current > 0) queued_audio_frames_.store(current - 1, std::memory_order_relaxed);
        } else {
            if (source == frame_source::pause_silence) {
                --pause_frames_remaining_;
            } else {
                ++underrun_ticks_;
            }
            stat_silence_frames_.fetch_add(1, std::memory_order_relaxed);
        }

        if (no_key_drop) {
            stat_no_key_drops_.fetch_add(1, std::memory_order_relaxed);
            has_prepared_packet_ = false;
            return true;
        }

        if (encrypted == discusy::voice::encrypt_result::ok) {
            const std::uint16_t be_seq = to_network_order(sequence_);
            std::memcpy(&rtp_header_[2], &be_seq, 2);
            const std::uint32_t be_timestamp = to_network_order(timestamp_);
            std::memcpy(&rtp_header_[4], &be_timestamp, 4);

            if (encrypt_voice_packet(rtp_header_, dave_buf_.span())) {
                has_prepared_packet_ = true;
            } else {
                stat_encrypt_drops_.fetch_add(1, std::memory_order_relaxed);
                has_prepared_packet_ = false;
            }
        } else {
            stat_encrypt_drops_.fetch_add(1, std::memory_order_relaxed);
            has_prepared_packet_ = false;
        }

        return true;
    }

    void tick_impl() {
        if (closed_ || !can_transmit()) {
            has_prepared_packet_ = false;
            park_drain();
            return;
        }

        if (has_prepared_packet_) {
            has_prepared_packet_ = false;
            if (!transmit_current_packet()) {
                park_drain();
                return;
            }
            ++sequence_;
        }

        timestamp_ += discusy::voice::TIMESTAMP_INCREMENT;
        advance_clock();

        if (!prepare_next_packet()) {
            if (draining_) {
                park_drain();
            }
            return;
        }

        arm_timer();
    }

    bool draining_{false};
    bool drain_pending_{false};
    bool has_prepared_packet_{false};

    bool idle_{true};
    bool paused_{false};
    bool armed_{false};
    bool keyed_{false};
    bool failed_{false};
    bool socket_broken_{false};
    std::uint32_t not_ready_retries_{0};
    std::uint32_t pause_frames_remaining_{0};
    std::uint32_t underrun_ticks_{0};

    std::atomic<std::uint32_t> underrun_grace_frames_{discusy::voice::DEFAULT_UNDERRUN_GRACE_FRAMES};
    std::atomic<std::uint32_t> max_catchup_frames_{discusy::voice::DEFAULT_MAX_CATCHUP_FRAMES};
    std::atomic<std::uint32_t> not_ready_hold_frames_{discusy::voice::DEFAULT_NOT_READY_HOLD_FRAMES};

    std::atomic<std::uint64_t> queued_audio_frames_{0};
    discusy::voice::audio_stop_reason next_idle_reason_{discusy::voice::audio_stop_reason::finished};

    ctx::io_context& io_ctx_;
    ctx::io_context::strand_t strand_;
    std::atomic_bool closed_{false};
    ctx::io_context::strand_udp_socket_t socket_;
    udp::endpoint remote_endpoint_;
    udp::endpoint discovery_sender_;

    std::uint64_t session_{0};
    std::uint64_t timer_gen_{0};

    std::array<std::uint8_t, 8192> recv_buffer_{};
    std::array<std::uint8_t, 8192> recv_decrypt_buf_{};

    discusy::voice::store<discusy::voice::MAX_DAVE_BUFFER_SIZE> dave_buf_; // more than enough
    discusy::voice::store<discusy::voice::MAX_FINAL_BUFFER_SIZE> next_buf_;

    std::uint16_t sequence_{0};
    std::uint32_t timestamp_{0};

    clock::time_point next_frame_tp_{};
    std::array<std::uint8_t, 12> rtp_header_{};

    std::uint32_t consecutive_send_errors_{0};
    std::uint32_t consecutive_recv_errors_{0};

    std::array<uint8_t, 74> discovery_packet_{}; // used for both send and recieve
    std::array<uint8_t, 32> secret_key_{};
    std::uint32_t ssrc_{0};
    std::uint32_t ssrc_net_order_{0};
    std::uint32_t nonce_counter_{0};
    encryption_mode mode_{encryption_mode::aead_xchacha20_poly1305_rtpsize};

    ctx::io_context::strand_udp_resolver_t resolver_;
    ctx::io_context::strand_timer_t timer_;

    const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES> silence_frame_;

    boost::container::deque<discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>, void, audio_options> audio_packets_;

    std::atomic<std::uint64_t> stat_frames_sent_{0};
    std::atomic<std::uint64_t> stat_silence_frames_{0};
    std::atomic<std::uint64_t> stat_underrun_events_{0};
    std::atomic<std::uint64_t> stat_underrun_frames_{0};
    std::atomic<std::uint64_t> stat_clock_resyncs_{0};
    std::atomic<std::uint64_t> stat_frames_skipped_{0};
    std::atomic<std::uint64_t> stat_send_errors_{0};
    std::atomic<std::uint64_t> stat_would_block_drops_{0};
    std::atomic<std::uint64_t> stat_encrypt_drops_{0};
    std::atomic<std::uint64_t> stat_not_ready_stalls_{0};
    std::atomic<std::uint64_t> stat_no_key_drops_{0};
    std::atomic<std::int64_t> stat_worst_lateness_us_{0};
    std::atomic<std::uint64_t> stat_packets_received_{0};
    std::atomic<std::uint64_t> stat_receive_decrypt_drops_{0};
};

}
