#include "net/http/client/client.h"

#include <cstring>

#include "boost/iostreams/copy.hpp"
#include "boost/iostreams/filter/gzip.hpp"
#include "boost/iostreams/filtering_streambuf.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/regex.hpp"

#include "net/ssl.h"
#include "net/tcp.h"

namespace asio = boost::asio;
using boost::system::error_code;

namespace net {
namespace http {
namespace client {

boost::regex chunk_size_rx_("\r?\n?[0-9a-fA-F]+\r\n");

template <typename C>
basic_http_client<C>::basic_http_client(
    asio::io_service& ios, url u, boost::posix_time::time_duration timeout)
    : C(ios, u.host(), u.port(), std::move(timeout)),
      response_stream_(&buf_),
      status_code_(0),
      length_(0) {}

template <typename C>
void basic_http_client<C>::query(request& req, callback cb) {
  if (!req.body.empty()) {
    auto content_length = boost::lexical_cast<std::string>(req.body.length());
    req.headers.insert(std::make_pair("Content-Length", content_length));
  }

  request_ = req.to_str();

  auto connect_cb =
      std::bind(&basic_http_client<C>::on_connect, this, std::move(cb),
                std::placeholders::_1, std::placeholders::_2);
  static_cast<C*>(this)->connect(std::move(connect_cb));
}

template <typename C>
void basic_http_client<C>::on_connect(callback cb, std::shared_ptr<C> self,
                                      error_code ec) {
  if (ec) {
    return cb(self, {std::map<std::string, std::string>(), ""}, ec);
  } else {
    return transfer(self, cb, ec);
  }
}

template <typename C>
std::streamsize basic_http_client<C>::read(char_type* s, std::streamsize n) {
  std::size_t ret = std::min(static_cast<std::size_t>(n), response_.size());
  if (ret != 0) {
    std::memcpy(s, &(response_[0]), ret);
    if (response_.size() != ret) {
      std::memmove(&(response_[0]), &(response_[ret]), response_.size() - ret);
    }
    response_.resize(response_.size() - ret);
  }
  return ret;
}

#include "boost/asio/yield.hpp"
template <typename C>
void basic_http_client<C>::transfer(std::shared_ptr<C> self, callback cb,
                                    error_code ec) {
  auto& my = *static_cast<C*>(this);

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
    auto re = std::bind(&basic_http_client<C>::transfer, this, self, cb, _1);
    std::size_t read, chunk_size, chunk_bytes, to_transfer, original;

    reenter(this) {
      yield asio::async_write(my.socket_, asio::buffer(request_), re);
      yield asio::async_read_until(my.socket_, buf_, "\r\n\r\n", re);

      read_header();

      if (header_.find("content-length") != header_.end()) {
        read = copy_content(buf_.size());

        try {
          read_content_length();
        } catch (std::bad_cast const&) {
          using namespace boost::system;
          error_code ec(errc::illegal_byte_sequence, system_category());
          return respond(cb, self, ec);
        }
        response_.resize(length_);

        if (length_ > read) {
          yield asio::async_read(
              my.socket_, asio::buffer(&(response_[read]), length_ - read),
              asio::transfer_at_least(length_ - read), re);
        }
      } else if (header_.find("transfer-encoding") != header_.end()) {
        while (true) {
          yield asio::async_read_until(my.socket_, buf_, chunk_size_rx_, re);

          chunk_size = 0;
          response_stream_ >> std::hex >> chunk_size;
          buf_.consume(2);

          if (chunk_size == 0) {
            break;
          }

          chunk_bytes = std::min(buf_.size(), chunk_size);
          copy_content(chunk_bytes);

          if (chunk_size > chunk_bytes) {
            to_transfer = chunk_size - chunk_bytes;
            original = response_.size();
            response_.resize(original + to_transfer);
            yield asio::async_read(
                my.socket_, asio::buffer(&(response_[original]), to_transfer),
                asio::transfer_at_least(to_transfer), re);
          }
        }
      } else {
        while (true) {
          copy_content(buf_.size());
          yield asio::async_read(my.socket_, buf_, asio::transfer_at_least(1),
                                 re);
        }
      }
      return respond(cb, self, ec);
    }
  }
}
#include "boost/asio/unyield.hpp"

template <typename C>
void basic_http_client<C>::respond(callback cb, std::shared_ptr<C> self,
                                   error_code ec) {
  static_cast<C*>(this)->finally(ec);

  if (asio::error::eof == ec) {
    ec = error_code();
  }

  typedef boost::reference_wrapper<basic_http_client> http_stream_ref;
  boost::iostreams::stream<http_stream_ref> s(boost::ref(*this));

  std::stringstream response;
  if (auto it = header_.find("content-encoding");
      it != header_.end() && it->second == "gzip") {
    try {
      boost::iostreams::filtering_streambuf<boost::iostreams::input> filter;
      filter.push(boost::iostreams::gzip_decompressor());
      filter.push(s);
      boost::iostreams::copy(filter, response);
    } catch (const boost::iostreams::gzip_error& e) {
      using namespace boost::system;
      error_code ec(errc::illegal_byte_sequence, system_category());
      return cb(self, {header_, ""}, ec);
    }
  } else {
    boost::iostreams::copy(s, response);
  }

  response_ = {};
  cb(self, {header_, response.str()}, ec);
}

template <typename C>
void basic_http_client<C>::read_header() {
  std::string http_version;
  response_stream_ >> http_version;
  response_stream_ >> status_code_;
  response_stream_.ignore(128, '\n');

  std::string header;
  while (std::getline(response_stream_, header) && header != "\r") {
    header = header.substr(0, header.length() - 1);
    std::size_t seperator_pos = header.find(": ");
    if (seperator_pos == std::string::npos) {
      continue;
    }

    std::string key = header.substr(0, seperator_pos);
    std::string key_to_lower;
    key_to_lower.resize(key.size());
    std::transform(key.begin(), key.end(), key_to_lower.begin(), ::tolower);
    std::string header_value = header.substr(seperator_pos + 2);

    if (key_to_lower == "set-cookie") {
      std::size_t val_end_pos = header_value.find(";");
      if (val_end_pos != std::string::npos) {
        header_["set-cookie"] += header_value.substr(0, val_end_pos + 1);
      }
    } else {
      header_[key_to_lower] = header_value;
    }
  }
}

template <typename C>
void basic_http_client<C>::read_content_length() {
  int l = boost::lexical_cast<int>(header_["content-length"]);
  // Check parse result: not negativ and <= 2MB
  if (l < 0 || l >= 0x200000) {
    throw std::bad_cast();
  }
  length_ = static_cast<std::size_t>(l);
}

template <typename C>
std::size_t basic_http_client<C>::copy_content(std::size_t buffer_size) {
  if (buffer_size > 0) {
    const char* buf = boost::asio::buffer_cast<const char*>(buf_.data());
    std::size_t original = response_.size();
    response_.resize(response_.size() + buffer_size);
    std::memcpy(&(response_[original]), buf, buffer_size);
    buf_.consume(buffer_size);
  }
  return buffer_size;
}

template class basic_http_client<ssl>;
template class basic_http_client<tcp>;

}  // namespace client
}  // namespace http
}  // namespace net