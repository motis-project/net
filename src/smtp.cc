#include "net/smtp.h"

#include <sstream>

#include "boost/asio.hpp"
#include "boost/date_time.hpp"
#include "boost/lexical_cast.hpp"

#include "net/base64.h"

#define SERVICE_READY (220)
#define REQUESTED_MAIL_ACTION_OK (250)
#define AUTHENTICATION_SUCCEEDED (235)
#define START_MAIL_INPUT (354)

namespace asio = boost::asio;
namespace pt = boost::posix_time;
using boost::system::error_code;

namespace net {

smtp_client::smtp_client(asio::io_service& ios, std::string host,
                         std::string port, std::string hostname,
                         boost::posix_time::time_duration timeout)
    : net::ssl(ios, std::move(host), std::move(port), std::move(timeout)),
      hostname_(std::move(hostname)),
      response_stream_(&buf_) {}

void smtp_client::query(smtp_request& req, callback cb) {
  generate_commands(req);
  connect([this, cb](std::shared_ptr<net::ssl> self, error_code ec) {
    if (ec) {
      return cb(self, ec);
    } else {
      return transfer(self, cb, ec);
    }
  });
}

#include "boost/asio/yield.hpp"
void smtp_client::transfer(std::shared_ptr<net::ssl> self, callback cb,
                           error_code ec) {
  if (ec == asio::error::eof) {
    boost::asio::detail::coroutine_ref(this) = 0;
    return respond(cb, self, ec);
  }

  if (ec) {
    return respond(cb, self, ec);
  } else {
    if (is_complete()) {
      boost::asio::detail::coroutine_ref(this) = 0;
    }

    using std::placeholders::_1;
    auto re = std::bind(&smtp_client::transfer, this, self, cb, _1);
    unsigned int status;

    reenter(this) {
      // Read service status.
      yield asio::async_read_until(socket_, buf_, "\r\n", re);
      response_stream_ >> status;
      if (status != SERVICE_READY) {
        return respond(cb, self, asio::error::operation_not_supported);
      }
      buf_.consume(buf_.size());

      // Initiate (send EHLO).
      yield asio::async_write(socket_, asio::buffer(init_cmd_), re);
      yield asio::async_read_until(socket_, buf_, "\r\n", re);
      response_stream_ >> status;
      if (status != REQUESTED_MAIL_ACTION_OK) {
        return respond(cb, self, asio::error::operation_not_supported);
      }
      buf_.consume(buf_.size());

      // Authenticate.
      yield asio::async_write(socket_, asio::buffer(auth_cmd_), re);
      yield asio::async_read_until(socket_, buf_, "\r\n", re);
      response_stream_ >> status;
      if (status != AUTHENTICATION_SUCCEEDED) {
        return respond(cb, self, asio::error::operation_not_supported);
      }
      buf_.consume(buf_.size());

      // Send MAIL FROM command.
      yield asio::async_write(socket_, asio::buffer(from_cmd_), re);
      yield asio::async_read_until(socket_, buf_, "\r\n", re);
      response_stream_ >> status;
      if (status != REQUESTED_MAIL_ACTION_OK) {
        return respond(cb, self, asio::error::operation_not_supported);
      }
      buf_.consume(buf_.size());

      // Send MAIL TO command.
      yield asio::async_write(socket_, asio::buffer(rcpt_cmd_), re);
      yield asio::async_read_until(socket_, buf_, "\r\n", re);
      response_stream_ >> status;
      if (status != REQUESTED_MAIL_ACTION_OK) {
        return respond(cb, self, asio::error::operation_not_supported);
      }
      buf_.consume(buf_.size());

      // Prepare for data.
      yield asio::async_write(socket_, asio::buffer(data_cmd_), re);
      yield asio::async_read_until(socket_, buf_, "\r\n", re);
      response_stream_ >> status;
      if (status != START_MAIL_INPUT) {
        return respond(cb, self, asio::error::operation_not_supported);
      }
      buf_.consume(buf_.size());

      // Send data.
      yield asio::async_write(socket_, asio::buffer(data_), re);
      yield asio::async_read_until(socket_, buf_, "\r\n", re);
      response_stream_ >> status;
      if (status != REQUESTED_MAIL_ACTION_OK) {
        return respond(cb, self, asio::error::operation_not_supported);
      }
      buf_.consume(buf_.size());

      // Quit.
      yield asio::async_write(socket_, asio::buffer(quit_cmd_), re);

      return respond(cb, self, ec);
    }
  }
}
#include "boost/asio/unyield.hpp"

void smtp_client::generate_commands(smtp_request const& req) {
  init_cmd_ = "EHLO client.example.com\r\n";

  std::string auth = req.username + '\0' + req.username + '\0' + req.password;
  auth_cmd_ = "AUTH PLAIN " + net::encode_base64(auth) + "\r\n";

  from_cmd_ = std::string("MAIL FROM:<") + req.from + ">\r\n";
  rcpt_cmd_ = std::string("RCPT TO:<") + req.to + ">\r\n";
  data_cmd_ = "DATA\r\n";

  std::stringstream data_stream;
  data_stream << "Date: "
              << pt::to_iso_extended_string(pt::second_clock::local_time())
              << "\r\n"
              << "From: <" + req.from + ">\r\n"
              << "To: <" + req.to + ">\r\n"
              << "Subject: " << req.subject << "\r\n\r\n"
              << req.content << "\r\n.\r\n";
  data_ = data_stream.str();

  quit_cmd_ = "QUIT\r\n";
}

void smtp_client::respond(callback cb, std::shared_ptr<net::ssl> self,
                          error_code ec) {
  finally(ec);

  if (asio::error::eof == ec) {
    ec = error_code();
  }

  return cb(self, ec);
}

}  // namespace net