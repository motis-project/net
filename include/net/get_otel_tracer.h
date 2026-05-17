#pragma once

#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/tracer.h"


namespace net {
 
inline opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> get_otel_tracer() {
    return opentelemetry::trace::Provider::GetTracerProvider()->GetTracer("net")
}

}  // namespace net