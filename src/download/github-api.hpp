#pragma once

#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

namespace github {

using WebResponse = geode::utils::web::WebResponse;

inline async::TaskHolder<WebResponse> s_requestHolder;

inline void get(
    std::string const& url,
    geode::Function<void(geode::Result<WebResponse>)> callback
) {
    s_requestHolder.spawn(
        geode::utils::web::WebRequest()
            .userAgent("DLL-Bot")
            .get(url),
        [callback = std::move(callback)](WebResponse response) mutable {
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

inline void download(
    std::string const& url,
    geode::Function<void(geode::Result<geode::ByteVector>)> callback
) {
    s_requestHolder.spawn(
        geode::utils::web::WebRequest()
            .userAgent("DLL-Bot")
            .get(url),
        [callback = std::move(callback)](WebResponse response) mutable {
            if (!response.ok()) {
                callback(geode::Err(
                    fmt::format("Download failed: HTTP {}", response.code())
                ));
                return;
            }

            callback(geode::Ok(response.data()));
        }
    );
}

}