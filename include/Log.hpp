/*===-- include/Log.hpp ----- Log System ----------------------------------===*\
|*                                                                            *|
|* Copyright (c) 2025 Ma Yuncong                                              *|
|* Licensed under the MIT License.                                            *|
|*                                                                            *|
|*===----------------------------------------------------------------------===*|
|*                                                                            *|
|* This header implement the log system.                                      *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

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

  /**
   * Log error message.
   *
   * @param msg error message.
   */
  inline void error(std::string_view msg)
  {
    Log::instance().error(msg);
  }
}
