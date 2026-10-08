#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <discusy/discusy.hpp>
#include <discusy/gateway_events.hpp>
#include <discusy/etf.hpp>
#include <discusy/json.hpp>
#include <etf/etf.hpp>

namespace bench_helpers {

template <typename T>
GLZ_ALWAYS_INLINE void do_not_optimize(T&& val) {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(val) : "memory");
#else
    volatile auto* p = &val;
    (void)p;
#endif
}

discusy::recieve_event::message_create make_medium_payload() {
    discusy::recieve_event::message_create m{};
    m.message.id = discusy::snowflake{112233445566778899ULL};
    m.message.channel_id = discusy::snowflake{223344556677889900ULL};
    m.other.guild_id = discusy::snowflake{334455667788990011ULL};
    
    m.message.author.id = discusy::snowflake{445566778899001122ULL};
    m.message.author.username = "DiscordPowerUser";
    m.message.author.discriminator = "0001";
    m.message.author.avatar = discusy::user_avatar_hash{"abcdef1234567890abcdef1234567890"};
    m.message.author.bot = false;
    
    m.message.content = "Benchmarking JSON vs ETF in discusy! High performance Discord bot library in modern C++23.";
    m.message.timestamp = std::chrono::system_clock::now();
    m.message.tts = false;
    m.message.mention_everyone = false;

    for (int i = 0; i < 5; ++i) {
        discusy::user::user u{};
        u.id = discusy::snowflake{500000000000000000ULL + static_cast<uint64_t>(i)};
        u.username = "User_" + std::to_string(i);
        u.discriminator = "000" + std::to_string(i);
        u.avatar = discusy::user_avatar_hash{"avatar_hash_" + std::to_string(i)};
        u.bot = (i % 2 == 0);
        m.message.mentions.emplace_back(std::move(u));
        m.message.mention_roles.emplace_back(600000000000000000ULL + static_cast<uint64_t>(i));
    }

    discusy::message::attachment att{};
    att.id = discusy::snowflake{556677889900112233ULL};
    att.filename = "benchmark_results.png";
    att.size = 204800;
    att.url = "https://cdn.discordapp.com/attachments/1/2/benchmark_results.png";
    att.proxy_url = "https://media.discordapp.net/attachments/1/2/benchmark_results.png";
    m.message.attachments.emplace_back(std::move(att));

    discusy::message::embed embed{};
    embed.title = "Benchmark Summary";
    embed.description = "This is a description! Testing native message_create in discusy.";
    embed.color = 0x5865F2;
    embed.fields.emplace();
    for (int i = 0; i < 8; ++i) {
        discusy::message::embed_field f{};
        f.name = "Metric Field #" + std::to_string(i);
        f.value = "Detailed value metric string for field " + std::to_string(i);
        f.inline_ = (i % 2 == 0);
        embed.fields->emplace_back(std::move(f));
    }
    m.message.embeds.emplace_back(std::move(embed));
    return m;
}

discusy::recieve_event::guild_members_chunk make_large_payload() {
    discusy::recieve_event::guild_members_chunk l{};
    l.guild_id = discusy::snowflake{998877665544332211ULL};
    l.chunk_index = 0;
    l.chunk_count = 1;

    for (int i = 0; i < 200; ++i) {
        discusy::guild::guild_member member{};
        discusy::user::user u{};
        u.id = discusy::snowflake{700000000000000000ULL + static_cast<uint64_t>(i)};
        u.username = "GuildMember_" + std::to_string(i);
        u.discriminator = "0";
        u.avatar = discusy::user_avatar_hash{"avatar_hash_" + std::to_string(i)};
        u.bot = false;
        member.user = std::move(u);
        if (i % 3 == 0) member.nick = "Nick_" + std::to_string(i);
        for (int r = 0; r < 4; ++r) {
            member.roles.emplace_back(800000000000000000ULL + static_cast<uint64_t>(r));
        }
        member.joined_at = std::chrono::system_clock::now();
        member.deaf = false;
        member.mute = false;
        l.members.emplace_back(std::move(member));
    }
    return l;
}

discusy::recieve_event::guild_create_struct make_xl_payload() {
    discusy::recieve_event::guild_create_struct g{};
    g.guild.id = discusy::snowflake{100000000000000001ULL};
    g.guild.name = "Mega Discord Community Guild";
    g.guild.icon = discusy::guild_icon_hash{"icon_hash_1234567890"};
    g.guild.owner_id = discusy::snowflake{100000000000000002ULL};
    g.guild.region = "us-east";
    g.guild.verification_level = discusy::guild::verification_level::MEDIUM;

    for (int i = 0; i < 40; ++i) {
        discusy::permissions::role r{};
        r.id = discusy::snowflake{850000000000000000ULL + static_cast<uint64_t>(i)};
        r.name = "Role_" + std::to_string(i);
        r.color = static_cast<discusy::integer>(i) * 1000;
        r.hoist = (i < 5);
        r.position = i;
        r.permissions = discusy::permissions_t{1071698660929ULL};
        r.mentionable = (i % 4 == 0);
        g.guild.roles.emplace_back(std::move(r));
    }

    for (int i = 0; i < 60; ++i) {
        discusy::channel::channel c{};
        c.id = discusy::snowflake{870000000000000000ULL + static_cast<uint64_t>(i)};
        c.type = (i % 5 == 0 ? discusy::channel::channel_type::GUILD_VOICE : discusy::channel::channel_type::GUILD_TEXT);
        c.name = "channel-" + std::to_string(i);
        c.position = i;
        if (i % 2 == 0) c.topic = "Topic for channel " + std::to_string(i);
        c.nsfw = false;
        c.rate_limit_per_user = (i % 10 == 0 ? 5 : 0);
        g.other.channels.emplace_back(std::move(c));
    }

    for (int i = 0; i < 800; ++i) {
        discusy::guild::guild_member member{};
        discusy::user::user u{};
        u.id = discusy::snowflake{900000000000000000ULL + static_cast<uint64_t>(i)};
        u.username = "CommunityMember_" + std::to_string(i);
        u.discriminator = "0";
        member.user = std::move(u);
        if (i % 4 == 0) member.nick = "DisplayName_" + std::to_string(i);
        for (int r = 0; r < 3; ++r) {
            member.roles.emplace_back(850000000000000000ULL + (static_cast<std::uint64_t>(r) * 2));
        }
        member.joined_at = std::chrono::system_clock::now();
        member.deaf = false;
        member.mute = false;
        g.other.members.emplace_back(std::move(member));
    }
    g.other.member_count = 800;
    g.other.joined_at = std::chrono::system_clock::now();

    return g;
}

struct BenchResult {
    std::string name;
    size_t json_size{0};
    size_t etf_size{0};
    double json_ser_ns{0.0};
    double etf_ser_ns{0.0};
    double json_deser_ns{0.0};
    double etf_deser_ns{0.0};
    double json_ser_mb_s{0.0};
    double etf_ser_mb_s{0.0};
    double json_deser_mb_s{0.0};
    double etf_deser_mb_s{0.0};
};

template <typename T>
BenchResult benchmark_payload(const std::string& name, const T& value, size_t iterations) {
    BenchResult res;
    res.name = name;

    std::string json_buffer;
    std::string etf_buffer;

    auto err_wj = glz::write_json(value, json_buffer);
    if (static_cast<bool>(err_wj)) {
        std::cerr << "\n[ERROR] JSON serialization failed for " << name << ": " << glz::format_error(err_wj, json_buffer) << "\n";
        return res;
    }
    auto err_we = glz::write_etf(value, etf_buffer);
    if (static_cast<bool>(err_we)) {
        std::cerr << "\n[ERROR] ETF serialization failed for " << name << ": " << glz::format_error(err_we, etf_buffer) << "\n";
        return res;
    }

    res.json_size = json_buffer.size();
    res.etf_size = etf_buffer.size();

    // Pre-flight deserialization validation
    {
        T test_target{};
        auto err_rj = glz::read_json(test_target, json_buffer);
        if (static_cast<bool>(err_rj)) {
            std::cerr << "\n[ERROR] Pre-flight JSON deserialization failed for " << name << ": " << glz::format_error(err_rj, json_buffer) << "\n";
            return res;
        }
        auto err_re = glz::read_etf(test_target, etf_buffer);
        if (static_cast<bool>(err_re)) {
            std::cerr << "\n[ERROR] Pre-flight ETF deserialization failed for " << name << ": " << glz::format_error(err_re, etf_buffer) << "\n";
            return res;
        }
    }

    // 1. JSON Serialization Benchmark
    {
        for (size_t i = 0; i < (iterations / 10) + 5; ++i) {
            json_buffer.clear();
            (void)glz::write_json(value, json_buffer);
            do_not_optimize(json_buffer);
        }
        const auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            json_buffer.clear();
            (void)glz::write_json(value, json_buffer);
            do_not_optimize(json_buffer);
        }
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        res.json_ser_ns = static_cast<double>(elapsed) / static_cast<double>(iterations);
        const double total_mb = (static_cast<double>(res.json_size) * static_cast<double>(iterations)) / (1024.0 * 1024.0);
        const double elapsed_s = static_cast<double>(elapsed) / 1e9;
        res.json_ser_mb_s = total_mb / elapsed_s;
    }

    // 2. ETF Serialization Benchmark
    {
        for (size_t i = 0; i < (iterations / 10) + 5; ++i) {
            etf_buffer.clear();
            (void)glz::write_etf(value, etf_buffer);
            do_not_optimize(etf_buffer);
        }
        const auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            etf_buffer.clear();
            (void)glz::write_etf(value, etf_buffer);
            do_not_optimize(etf_buffer);
        }
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        res.etf_ser_ns = static_cast<double>(elapsed) / static_cast<double>(iterations);
        const double total_mb = (static_cast<double>(res.etf_size) * static_cast<double>(iterations)) / (1024.0 * 1024.0);
        const double elapsed_s = static_cast<double>(elapsed) / 1e9;
        res.etf_ser_mb_s = total_mb / elapsed_s;
    }

    // 3. JSON Deserialization Benchmark
    {
        T target{};
        for (size_t i = 0; i < (iterations / 10) + 5; ++i) {
            (void)glz::read_json(target, json_buffer);
            do_not_optimize(target);
        }
        const auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            (void)glz::read_json(target, json_buffer);
            do_not_optimize(target);
        }
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        res.json_deser_ns = static_cast<double>(elapsed) / static_cast<double>(iterations);
        const double total_mb = (static_cast<double>(res.json_size) * static_cast<double>(iterations)) / (1024.0 * 1024.0);
        const double elapsed_s = static_cast<double>(elapsed) / 1e9;
        res.json_deser_mb_s = total_mb / elapsed_s;
    }

    // 4. ETF Deserialization Benchmark
    {
        T target{};
        for (size_t i = 0; i < (iterations / 10) + 5; ++i) {
            (void)glz::read_etf(target, etf_buffer);
            do_not_optimize(target);
        }
        const auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            (void)glz::read_etf(target, etf_buffer);
            do_not_optimize(target);
        }
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        res.etf_deser_ns = static_cast<double>(elapsed) / static_cast<double>(iterations);
        const double total_mb = (static_cast<double>(res.etf_size) * static_cast<double>(iterations)) / (1024.0 * 1024.0);
        const double elapsed_s = static_cast<double>(elapsed) / 1e9;
        res.etf_deser_mb_s = total_mb / elapsed_s;
    }

    return res;
}

std::string format_time(double ns) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    if (ns < 1000.0) {
        ss << ns << " ns";
    } else if (ns < 1000000.0) {
        ss << (ns / 1000.0) << " us";
    } else {
        ss << (ns / 1000000.0) << " ms";
    }
    return ss.str();
}

void print_table_row(const BenchResult& r) {
    std::cout << "\n========================================================================================\n";
    std::cout << " Payload: " << r.name << "\n";
    std::cout << " Sizes  : JSON: " << r.json_size << " bytes | ETF: " << r.etf_size << " bytes (";
    const double size_diff = 100.0 * (1.0 - (static_cast<double>(r.etf_size) / static_cast<double>(r.json_size)));
    if (size_diff >= 0) {
        std::cout << "-" << std::fixed << std::setprecision(1) << size_diff << "% smaller)\n";
    } else {
        std::cout << "+" << std::fixed << std::setprecision(1) << -size_diff << "% larger)\n";
    }
    std::cout << "----------------------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(18) << "Operation"
              << std::setw(16) << "JSON Latency"
              << std::setw(16) << "ETF Latency"
              << std::setw(16) << "JSON MB/s"
              << std::setw(16) << "ETF MB/s"
              << "Speedup\n";
    std::cout << "----------------------------------------------------------------------------------------\n";

    // Serialize
    const double ser_speedup = r.json_ser_ns / (r.etf_ser_ns > 0 ? r.etf_ser_ns : 1.0);
    std::cout << std::left << std::setw(18) << "Serialize"
              << std::setw(16) << format_time(r.json_ser_ns)
              << std::setw(16) << format_time(r.etf_ser_ns)
              << std::setw(16) << (std::to_string(static_cast<int>(r.json_ser_mb_s)) + " MB/s")
              << std::setw(16) << (std::to_string(static_cast<int>(r.etf_ser_mb_s)) + " MB/s")
              << std::fixed << std::setprecision(2) << ser_speedup << "x "
              << (ser_speedup >= 1.0 ? "(ETF faster)" : "(JSON faster)") << "\n";

    // Deserialize
    const double deser_speedup = r.json_deser_ns / (r.etf_deser_ns > 0 ? r.etf_deser_ns : 1.0);
    std::cout << std::left << std::setw(18) << "Deserialize"
              << std::setw(16) << format_time(r.json_deser_ns)
              << std::setw(16) << format_time(r.etf_deser_ns)
              << std::setw(16) << (std::to_string(static_cast<int>(r.json_deser_mb_s)) + " MB/s")
              << std::setw(16) << (std::to_string(static_cast<int>(r.etf_deser_mb_s)) + " MB/s")
              << std::fixed << std::setprecision(2) << deser_speedup << "x "
              << (deser_speedup >= 1.0 ? "(ETF faster)" : "(JSON faster)") << "\n";
    std::cout << "========================================================================================\n";
}

} // namespace bench_helpers

using namespace bench_helpers;

int main() {
    std::cout << R"(
             JSON vs ETF Performance Benchmark (All Payload Sizes)

 Hardware & Architecture: )" << sizeof(void*) * 8 << "-bit, "
              << (std::endian::native == std::endian::little ? "Little-Endian" : "Big-Endian")
              << "\n Running benchmark iterations using native discusy library types...\n";

    // 1. Small Tier
    std::cout << "\n[1/4] Benchmarking Small Payloads (Heartbeat & Presence)..." << std::flush;
    discusy::recieve_event::payload<discusy::recieve_event::hello> hb{};
    hb.d.heartbeat_interval = 41250;
    auto res_hb = benchmark_payload("Small: Gateway Hello / Heartbeat", hb, 50000);
    print_table_row(res_hb);

    discusy::send_event::update_presence presence{};
    presence.status = discusy::send_event::status_type::online;
    presence.afk = false;
    discusy::recieve_event::presence::activity act{};
    act.name = "Playing discusy";
    act.type = discusy::recieve_event::presence::activity_type::playing;
    presence.activities.emplace_back(std::move(act));
    auto res_pres = benchmark_payload("Small: Presence Update", presence, 30000);
    print_table_row(res_pres);

    // 2. Medium Tier
    std::cout << "\n[2/4] Benchmarking Medium Payloads (Message Create with Mentions & Embeds)..." << std::flush;
    auto msg = make_medium_payload();
    auto res_msg = benchmark_payload("Medium: Message Create", msg, 10000);
    print_table_row(res_msg);

    // 3. Large Tier
    std::cout << "\n[3/4] Benchmarking Large Payloads (200 Guild Members Sync)..." << std::flush;
    auto large = make_large_payload();
    auto res_large = benchmark_payload("Large: 200 Members List", large, 2000);
    print_table_row(res_large);

    // 4. Extra-Large Tier
    std::cout << "\n[4/4] Benchmarking Extra-Large Payloads (Guild Create: 800 Members, 60 Channels, 40 Roles)..." << std::flush;
    auto xl = make_xl_payload();
    auto res_xl = benchmark_payload("Extra-Large: Full Guild Create", xl, 400);
    print_table_row(res_xl);

    std::cout << "\nBenchmark complete!\n";
    return 0;
}
