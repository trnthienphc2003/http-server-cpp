#include <http_server/router.hpp>
#include <iostream>

void http_server::Router::add_route(const std::string &method, const std::string &path_pattern, Handler handler) {
    try {
        auto [compiled_method, compiled_path] = compile_path_pattern(path_pattern);
        routes.push_back({method, path_pattern, std::move(compiled_method), std::move(compiled_path), std::move(handler)});
    } catch (const std::exception& e) {
        std::cerr << "Error adding route: " << e.what() << std::endl;
    }
}

http_server::HTTP_Response http_server::Router::dispatch(const HTTP_Request &request) const {
    try {
        for (const auto &route : routes) {
            if(route.method != request.method) {
                continue; // Skip if method doesn't match
            }
            std::smatch match;
            if (std::regex_match(request.path, match, route.regex_pattern)) {
                Params params;
                for (size_t i = 0; i < route.path_params.size(); ++i) {
                    params[route.path_params[i]] = match[i + 1].str();
                }
                return route.handler(request, params);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error dispatching request: " << e.what() << std::endl;
    }
    return HTTP_Response{
        static_cast<int>(HTTP_STATUS_CODE::NOT_FOUND),
        "Not Found",
        {},
        ""
    };
}

std::pair<std::regex, std::vector<std::string>> http_server::Router::compile_path_pattern(const std::string &path_pattern) {
    std::string regex_str = "^";
    std::vector<std::string> names;
    size_t i = 0;
    while (i < path_pattern.size()) {
        if (path_pattern[i] == ':' && (i+1)<path_pattern.size()) {
            // parameter  
            size_t j = i+1;
            while (j < path_pattern.size() && path_pattern[j] != '/') ++j;
            names.push_back(path_pattern.substr(i+1, j-(i+1)));
            regex_str += "([^/]+)";
            i = j;
        } else {
            // escape regex chars
            if (std::string(".^$|()[]*+?{}\\").find(path_pattern[i]) != std::string::npos)
                regex_str += '\\';
            regex_str += path_pattern[i++];
        }
    }
    regex_str += "$";
    return { std::regex(regex_str), names };
}