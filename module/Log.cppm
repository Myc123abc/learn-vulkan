module;

#include <spdlog/spdlog.h>

#include <string_view>

export module Log;

namespace Log
{
  class Log final
  {
    friend Log& instance() noexcept;

    Log()
    {
      spdlog::set_pattern("%^%L:%$ %v");
    }

  public:
    void error(std::string_view msg)
    {
      spdlog::error(msg.data());
    }
  };

  inline Log& instance() noexcept
  {
    static Log log;
    return log;
  }

  export void error(std::string_view msg)
  {
    instance().error(msg);
  }
}
