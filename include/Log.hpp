#pragma once

#include <spdlog/spdlog.h>

#include <string_view>

namespace Log
{
  class Log final
  {
    friend void error(std::string_view msg);

    static Log& instance() noexcept
    {
      static Log log;
      return log;
    }

    Log()
    {
      spdlog::set_pattern("%^%L:%$ %v");
    }

    void error(std::string_view msg)
    {
      spdlog::error(msg.data());
    }
  };

  inline void error(std::string_view msg)
  {
    Log::instance().error(msg);
  }
}
