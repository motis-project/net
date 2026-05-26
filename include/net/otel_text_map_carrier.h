#pragma once

#include <type_traits>

#include "opentelemetry/context/propagation/text_map_propagator.h"

namespace net {

template <typename T>
struct http_text_map_carrier
    : public opentelemetry::context::propagation::TextMapCarrier {
  explicit http_text_map_carrier(T& header) : header_{header} {}

  virtual opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view key) const noexcept override {
    return header_[key];
  }

  virtual void Set(opentelemetry::nostd::string_view key,
                   opentelemetry::nostd::string_view value) noexcept override {
    if constexpr (!std::is_const_v<T>) {
      header_.set(key, value);
    }
  }

  T& header_;
};

}  // namespace net