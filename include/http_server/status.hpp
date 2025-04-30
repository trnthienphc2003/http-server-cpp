#ifndef STATUS_HPP
#define STATUS_HPP

namespace http_server {
    enum class HTTP_STATUS_CODE{
        OK                      = 200,
        CREATED                 = 201,
        ACCEPTED                = 202,
        NO_CONTENT              = 204,
        MOVED_PERMANENTLY       = 301,
        FOUND                   = 302,
        SEE_OTHER               = 303,
        NOT_MODIFIED            = 304,
        BAD_REQUEST             = 400,
        UNAUTHORIZED            = 401,
        FORBIDDEN               = 403,
        NOT_FOUND               = 404,
        METHOD_NOT_ALLOWED      = 405,
        INTERNAL_SERVER_ERROR   = 500,
        NOT_IMPLEMENTED         = 501,
        BAD_GATEWAY             = 502,
        SERVICE_UNAVAILABLE     = 503,
    };
}

#endif