#pragma once

#include <Geode/utils/web.hpp>

namespace github {

inline void get(
    std::string const& url,
    std::function<void(geode::Result<geode::utils::web::WebResponse>)> callback
) {
    geode::async::spawn(
        geode::utils::web::WebRequest()
            .userAgent("DLL-Bot")
            .get(url),
        [callback](geode::utils::web::WebResponse response) {
            if (!response.ok()) {
                callback(geode::Err(
                    fmt::format("HTTP request failed: {}", response.code())
                ));
                return;
            }

            callback(geode::Ok(std::move(response)));
        }
    );
}

}
