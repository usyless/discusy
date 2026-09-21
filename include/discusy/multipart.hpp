#pragma once

#include <string>
#include <string_view>
#include <span>

namespace discusy::multipart {

inline constexpr auto BOUNDARY_HEADER = "multipart/form-data; boundary=---------DiscusyBoundary1234-5"; // two less --
inline constexpr auto BOUNDARY = "-----------DiscusyBoundary1234-5";

constexpr void add_multipart_part(std::string& body, const std::string_view name, const std::string_view content, 
                    const std::string_view filename = "", const std::string_view type = "") {
    body.append(BOUNDARY).append("\r\nContent-Disposition: form-data; name=\""); // extra two --
    body.append(name).append("\"");
    
    if (!filename.empty()) {
        body.append("; filename=\"").append(filename).append("\"");
    }
    
    body.append("\r\n");
    
    if (!type.empty()) {
        body.append("Content-Type: ").append(type).append("\r\n");
    }
    
    body.append("\r\n").append(content).append("\r\n");
}

template <typename ElementType, std::size_t Extent>
constexpr void add_multipart_part(std::string& body, const std::string_view name, const std::span<ElementType, Extent> content, 
                    const std::string_view filename = "", const std::string_view type = "") {
    const std::string_view view = content.empty() 
        ? std::string_view{} 
        : std::string_view{reinterpret_cast<const char*>(content.data()), content.size_bytes()};
    add_multipart_part(body, name, view, filename, type);
}

constexpr void finish_multipart(std::string& body) {
    body.append(BOUNDARY).append("--\r\n");
}

}
