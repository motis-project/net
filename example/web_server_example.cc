#include <cstddef>
#include <iostream>
#include <memory>

#include "boost/asio/buffer.hpp"
#include "boost/asio/ssl/context.hpp"
#include "boost/beast/version.hpp"

#include "net/stop_handler.h"
#include "net/web_server/web_server.h"

using namespace net;
using net::web_server;

inline void load_server_certificate(boost::asio::ssl::context& ctx) {
  std::string const cert =
      R"(-----BEGIN CERTIFICATE-----
MIIFiDCCA3CgAwIBAgIJALTvhhalAZPmMA0GCSqGSIb3DQEBCwUAMFkxCzAJBgNV
BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX
aWRnaXRzIFB0eSBMdGQxEjAQBgNVBAMMCWxvY2FsaG9zdDAeFw0xODA3MjYxMjM1
MDNaFw0xOTA3MjYxMjM1MDNaMFkxCzAJBgNVBAYTAkFVMRMwEQYDVQQIDApTb21l
LVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQxEjAQBgNV
BAMMCWxvY2FsaG9zdDCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAKSy
8js7tr0Y57WA/bPDDFqdjPCC85HFPB3v+72fD8efZ/uvxjI3Sj66UvWLi8u5MWwz
o6R8VFcQ9W/zC9iJtMVgiyJUhsHIR9KFgPNZxR/gbMFvTqrjCFTC9VNavCgmmnCv
uYRYNHvcUUoZviyOtQazsiqckCW5GHTyHLbto+I5CzoNG3IOxDu0pS+sQOPBY9eu
OCcOD0puJDrrAsHXopRkejBqc65iy0HwbX/TbwEoJ9qpU0xAOoAMsjnOD4a2cwY1
tPvwMNMxK/oicIPKCRD1ei5L7iT/XOgXJVEq1VboEafzqDoAowehoSoSyWNYARY3
baZsW4DOVjTLTqw/Dp7eLcxVH/VK1KnPj1XaJtrf+vhkKGRPtzvXWF4F/fWtty1l
Q4bGRPv2Qkr764iuS0VTJhscRALyeRv1OBvxi1Llv4UqBFVXJj5Eg3A0ZrMj9I3T
fwaL44luGNG3Jsr8KUimYVO7G+lVEC6MAvdw/EqmwTD6M075ufYxU2CfZPdTq0Et
PvdTxcUfb++jCsO3fTJLQlPoRdA1+ZgVzlOPmE6IPxg8X6ag3V1w1JLKob0d5h3J
q4jtzdpBC2e83IRtGyR6bQ4EI2cdG15kh37g5wZ8NHkYR94RmVs/Jmou6n23ywon
dOcobp10o1oCCCkTjDRbUp4zvXr35XbkTZKZ5fp7AgMBAAGjUzBRMB0GA1UdDgQW
BBQs83SNinMYW7207h+JdVN5Nw1/AjAfBgNVHSMEGDAWgBQs83SNinMYW7207h+J
dVN5Nw1/AjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4ICAQBLX60b
Ql1gH8UjMDQDo73zskqGJwtgMdIf4cP12gYCcuVR98eSQ8fJNgGKtYZMycxkSylU
wcNArJf5jlw3rdgI94IiVDtm+N8xtK0oFrKgVQXW1GGn1YrsIlxiYUUn4FGMadeo
MvjcyJ590q9ov1k+iBNWFvQBU6jXMcDwYwP1Xqdo1uTmfK7FWHABJvB5ccHzFw8U
eEGxKevGjBxEOJcmS943Qr5zlBcIpRLrFfwGE3BQ5xatMg//tkNCE5BWScnXsW5o
an5JgRTeOFQAGmZvBwm1bEBLHOfvbuaYZOIn1CJeWmYdw9wOnlniyD4MFGl7X+Q0
a7rPPKcpZ78005tKUIyIq+m5LljCSsIzXjncp9/VUVzMe/94lWhtlu7K77BEuGHF
AqF5+tdDGNbOr3pWuBMnEwnfNBP+HkpV5b7d9hwnlE+bqXwcEfpkhg1zHxXkz0lF
ohR39fKHNo9UkCzZBsubO7RQnkpZwSjDSdO+8J+gAYDFNSjlqPdV/Ykrb8Wsr2y7
Jd9sO4+Y3lhd2sbcGXsnChYhk4y8uN4aQ24JUS21heKfpfT6zqOBojYjzVKkQw4B
QXsSUUcigWgH0FYP4i8y0zWW31f76BVxfcJErJu+C9Gh/uroVYt3HwKRFJuKM/6y
i0so3d4t/4TrWvJjBEPq/oVEUzNsMjvL7NfiOg==
-----END CERTIFICATE-----
)";

  std::string const key =
      R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
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
tpjBbbXmnNVum1thO35VaA==
-----END ENCRYPTED PRIVATE KEY-----
)";

  std::string const dh =
      "-----BEGIN DH PARAMETERS-----\n"
      "MIIBCAKCAQEArzQc5mpm0Fs8yahDeySj31JZlwEphUdZ9StM2D8+Fo7TMduGtSi+\n"
      "/HRWVwHcTFAgrxVdm+dl474mOUqqaz4MpzIb6+6OVfWHbQJmXPepZKyu4LgUPvY/\n"
      "4q3/iDMjIS0fLOu/bLuObwU5ccZmDgfhmz1GanRlTQOiYRty3FiOATWZBRh6uv4u\n"
      "tff4A9Bm3V9tLx9S6djq31w31Gl7OQhryodW28kc16t9TvO1BzcV3HjRPwpe701X\n"
      "oEEZdnZWANkkpR/m/pfgdmGPU66S2sXMHgsliViQWpDCYeehrvFRHEdR9NV+XJfC\n"
      "QMUk26jPTIVTLfXmmwU0u8vUkpR7LQKkwwIBAg==\n"
      "-----END DH PARAMETERS-----\n";

  ctx.set_password_callback(
      [](std::size_t, boost::asio::ssl::context_base::password_purpose) {
        return "test";
      });

  ctx.set_options(boost::asio::ssl::context::default_workarounds |
                  boost::asio::ssl::context::no_sslv2 |
                  boost::asio::ssl::context::single_dh_use);

  ctx.use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));

  ctx.use_private_key(boost::asio::buffer(key.data(), key.size()),
                      boost::asio::ssl::context::file_format::pem);

  ctx.use_tmp_dh(boost::asio::buffer(dh.data(), dh.size()));
}

int main() {
  namespace ssl = boost::asio::ssl;
  ssl::context ctx{ssl::context::tlsv12};
  load_server_certificate(ctx);

  boost::asio::io_context ioc;
  web_server s{ioc, ctx};

  s.on_ws_msg([](ws_session_ptr s, std::string const& msg, ws_msg_type type) {
    std::cout << "received: \"" << msg << "\"" << std::endl;
    s.lock()->send(msg, type, [](boost::system::error_code ec, size_t) {
      if (ec) {
        std::cout << "send ec: " << ec.message() << "\n";
      }
    });
  });
  s.on_ws_open([](ws_session_ptr s, bool ssl) {
    std::cout << "session open: " << s.lock().get() << " ssl=" << ssl << "\n";
  });
  s.on_ws_close([](void* s) { std::cout << "session close: " << s << "\n"; });
  s.on_http_request([](web_server::http_req_t const& req,
                       web_server::http_res_cb_t const& cb, bool ssl) {
    boost::ignore_unused(ssl);
    auto const server_error = [&req](std::string const& what) {
      web_server::string_res_t res{
          boost::beast::http::status::internal_server_error, req.version()};
      res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
      res.set(boost::beast::http::field::content_type, "text/html");
      res.keep_alive(req.keep_alive());
      res.body() = "An error occurred: '" + what + "'";
      res.prepare_payload();
      return res;
    };
    return cb(server_error("NOTHING TO SEE HERE - GO AWAY!"));
  });
  boost::system::error_code ec;
  s.init("0.0.0.0", "9000", ec);
  if (ec) {
    std::cout << "init error: " << ec.message() << "\n";
    return 1;
  }
  stop_handler stop(ioc, [&]() {
    s.stop();
    ioc.stop();
  });
  std::cout << "web server running\n";
  s.run();
  ioc.run();
}