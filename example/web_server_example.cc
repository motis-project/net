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
MIIFCTCCAvGgAwIBAgIUcQbgH9NsAeGOcfAtj7+YnOnOuiwwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MB4XDTE5MDgwNzA5NTY1OVoXDTI5MDgw
NDA5NTY1OVowFDESMBAGA1UEAwwJbG9jYWxob3N0MIICIjANBgkqhkiG9w0BAQEF
AAOCAg8AMIICCgKCAgEAsQO2XY2Fu6P7I7DFTxbq72yCPyaXXRRB/dqK0UEiKkUM
jK9XwdIWfKKeu63FYZOBZ/oxOCX2CAZlcRXdFKvva1p6lJWf4JsMjn6inYxOW8oS
SL0TIhQV4+C9aqkuhG77JLdVzKODczgbn4Kf1NoHjf/qNZTIkYOvnRObpIu+V6sE
SLz9lGcUPZyAxV54zcl+hyYwyhK8/ChpwPgYTjwqmrGXZkxdgk8sov+TlPEkLkXH
SW+2fHrV7rJdloedCEHNrYGmFd0p1NTEb16PGYr5A4iyZ5FANEe9BKZA3OzCfmSc
pQR9N/t41tYEK0sVHPSYVgGpyZ/eM0D2TLexUMdGpWqQw3umjoyb8jGfKaUw8P4t
XgDd5Qe4Nh/hxeQSCMfuG7qZTPLwdJAGr3MyrN/kZgkF2Be+FIGWIwULaPzL8TiY
hc8re6MLsz+QP340Df7GKwOaX3Tg+5SSVdJcDEew8AKm6d2tgL7+UoxXcGy52CFW
x1M6aKYMbE47LcsDTsf4CN+CmSu6uCbZl+xfQ2qP2qXPzKCHHvXH+ylwDiQKa11u
DQ0OzYT07RpDTwtn1jqUdYMPxLXPs7YNNpP1VFYd2faEg5WcXiaWYyV0g5u0RcVe
XikKO2D6zG+eMR73achzucvu9QhAYj2kZZ6FwOde9783jEikLi4UdliMi0gsDwcC
AwEAAaNTMFEwHQYDVR0OBBYEFB51EcOTBb3I7jEtnlroIzPpo8ANMB8GA1UdIwQY
MBaAFB51EcOTBb3I7jEtnlroIzPpo8ANMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZI
hvcNAQELBQADggIBADFgxkyfFk0rqe5pJ3KII11ufegnaPkAmwDxRPvSKTR2Cfqb
4u1pAph9kLgdx1ErRx1ZAqWT+Q/TYgkjdV/8SimGuZTAQ5UTeOV1Grb2wAEGr7ST
vc31kMI7f5KBKja4Mfxb91Y4kA8b1uunvORGheot2SGIlRTXgzFwG+dmkoCb8wEz
DsMaM+4woAe9X1WPRRcyuyI6+laG5acjHtSXNS6oklKuESmwcrsMjpnCDWm8nm7p
NrVpCDtV6hVU81X+eb0okAAGkD43qzr5HHVoakmgbsdQf7n9Hb9juQNNiPRX/34U
yqiiaVnzKxo1Z31ATIfpZAt7waX+OkJqOS6YBwvCCV82EueYrJ9e3JlDnlkbRQRq
xVQleWGwJve3P2JR8SHJ3ZcE+lf1OhPBUDYiEYhiejyaqTbuRhkESqJZlXbgV+Vz
RD4yJZT1rIYqisfH0qqu6cpnrGWA0l/kzZogSd3k04mTr/UFy5g8Bs4m+p3TD1+9
Axo1nXQ11oCnPo6+pWNrBHvtFurDJ5S7rEkGZISKHH8PmVIGs71d2SeT4KuHGXv3
Nn9PQEk9bSym5/DCcfhQOCqf7OFJe0c0USS2LNwDizwJPQaYaPrhlWNPbySSBJNC
e7znZhwXPN6wTgt61a7n488TyJBzhMXucDuRH0oQKLT/YgZyDeUOJCFdZjjF
-----END CERTIFICATE-----
)";

  std::string const key =
      R"(-----BEGIN PRIVATE KEY-----
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
PuEU2SmQuRhF30KSQITDtTRD99+H4g==
-----END PRIVATE KEY-----
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

  s.on_ws_msg(
      [](ws_session_ptr const& s, std::string const& msg, ws_msg_type type) {
        std::cout << "received: \"" << msg << "\"" << std::endl;
        s.lock()->send(msg, type, [](boost::system::error_code ec, size_t) {
          if (ec) {
            std::cout << "send ec: " << ec.message() << "\n";
          }
        });
      });
  s.on_ws_open([](ws_session_ptr const& s, bool ssl) {
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