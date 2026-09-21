#include <catch2/catch_test_macros.hpp>
#include <discusy/multipart.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

TEST_CASE("Multipart: Multipart MIME boundary and header construction", "[multipart]") {
    std::string body;
    const std::string_view json_content = R"({"content":"Hello with attachment"})";
    const std::vector<std::uint8_t> binary_data{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}; // PNG magic header

    // 1. Add payload_json part
    discusy::multipart::add_multipart_part(body, "payload_json", json_content, "", "application/json");

    // 2. Add binary attachment part using std::span overload
    discusy::multipart::add_multipart_part(
        body,
        "files[0]",
        std::span<const std::uint8_t>(binary_data),
        "image.png",
        "image/png"
    );

    // 3. Finalize
    discusy::multipart::finish_multipart(body);

    // Verification
    CHECK(body.starts_with(discusy::multipart::BOUNDARY));

    // Check payload_json formatting
    CHECK(body.contains("Content-Disposition: form-data; name=\"payload_json\"\r\n"));
    CHECK(body.contains("Content-Type: application/json\r\n\r\n" + std::string(json_content) + "\r\n"));

    // Check binary file attachment formatting
    CHECK(body.contains("Content-Disposition: form-data; name=\"files[0]\"; filename=\"image.png\"\r\n"));
    CHECK(body.contains("Content-Type: image/png\r\n\r\n"));

    // Check terminating boundary
    std::string expected_terminator = std::string(discusy::multipart::BOUNDARY) + "--\r\n";
    CHECK(body.ends_with(expected_terminator));
}
