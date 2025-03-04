// Copyright (c) 2018-present, Chang Liu <github.com/slontia>. All rights reserved.
//
// This source code is licensed under LGPLv2 (found in the LICENSE file).

#if !defined(OPTION_FILENAME) || !defined(OPTION_CLASSNAME)
#error OPTION_FILENAME and OPTION_CLASSNAME is not defined
#endif

#define INIT_OPTION_DEPEND
#define EXTEND_OPTION(_0, _1, _2, _3)
#include OPTION_FILENAME
#undef INIT_OPTION_DEPEND
#undef EXTEND_OPTION

#ifndef GET_OPTION_VALUE
#define GET_OPTION_VALUE(option, name) (option).Option2Value<std::decay_t<decltype(option)>::OPTION_##name>()
#endif

#ifndef GET_VALUE
#define GET_VALUE(name) GET_OPTION_VALUE(*this, name)
#endif

#define OPTION_(name) OPTION_##name
#define CHECKER_(name) (std::get<OPTION_(name) * 2 + 1>(options_))
#define VALUE_(name) (std::get<OPTION_(name) * 2>(options_))

class OPTION_CLASSNAME
{
  public:
    enum Option : int {
        INVALID_OPTION = -1,
#define EXTEND_OPTION(_0, name, _1, _2) OPTION_(name),
#include OPTION_FILENAME
#undef EXTEND_OPTION
        MAX_OPTION
    };

    OPTION_CLASSNAME() : options_
    {
#define EXTEND_OPTION(_0, _1, checker, default_value) \
        default_value, std::move(checker),
#include OPTION_FILENAME
#undef EXTEND_OPTION
    }, info_funcs_
    {
#define EXTEND_OPTION(description, name, _0, _1) \
        [this](const bool with_color) -> std::string { \
            const auto format_head = description " \n    - 格式：" #name " "; \
            const auto example = " \n    - 例如：" #name " " + CHECKER_(name).ExampleInfo(); \
            return format_head + (with_color ? CHECKER_(name).ColoredFormatInfo() : CHECKER_(name).FormatInfo()) + example + "\n    - 当前：" + \
            (with_color ? HTML_COLOR_FONT_HEADER(red) + CHECKER_(name).ArgString(VALUE_(name)) + HTML_FONT_TAIL \
                        : CHECKER_(name).ArgString(VALUE_(name))); \
        },
#include OPTION_FILENAME
#undef EXTEND_OPTION
    }
    {
        for (size_t i = 0; i < Option::MAX_OPTION; ++i) {
            colored_infos_[i] = info_funcs_[i](true);
            infos_[i] = info_funcs_[i](false);
        }
    }

    template <Option op>
    auto& Option2Value()
    {
        static_assert(op != Option::INVALID_OPTION, "Unexpected option");
        return std::get<op * 2>(options_);
    }

    template <Option op>
    const auto& Option2Value() const
    {
        static_assert(op != Option::INVALID_OPTION, "Unexpected option");
        return std::get<op * 2>(options_);
    }

    bool SetOption(MsgReader& msg_reader)
    {
        // TODO: use std::map to optimize
#define EXTEND_OPTION(_0, name, _1, _2) \
        static_assert(!std::is_void_v<std::decay<decltype(CHECKER_(name))>::type::arg_type>, \
                    "Checker cannot be void checker"); \
        if (msg_reader.Reset(); #name == msg_reader.NextArg()) { \
            auto value = CHECKER_(name).Check(msg_reader); \
            if (value.has_value() && !msg_reader.HasNext()) { \
                VALUE_(name) = *value; \
                colored_infos_[OPTION_(name)] = info_funcs_[OPTION_(name)](true); \
                infos_[OPTION_(name)] = info_funcs_[OPTION_(name)](false); \
                return true; \
            } \
        }
#include OPTION_FILENAME
#undef EXTEND_OPTION
        return false;
    }

    constexpr uint32_t Count() const { return Option::MAX_OPTION; }
    const char* Info(const uint64_t index) const { return infos_[index].c_str(); }
    const char* ColoredInfo(const uint64_t index) const { return colored_infos_[index].c_str(); }

  private:
    decltype(std::tuple{
#define EXTEND_OPTION(_0, _1, checker, default_value) \
    static_cast<std::decay<decltype(checker)>::type::arg_type>(default_value), checker,
#include OPTION_FILENAME
#undef EXTEND_OPTION
    }) options_;

    std::array<std::function<std::string(bool)>, Option::MAX_OPTION> info_funcs_;
    std::array<std::string, Option::MAX_OPTION> infos_;
    std::array<std::string, Option::MAX_OPTION> colored_infos_;
};

#undef OPTION_
#undef VALUE_
#undef CHECKER_
