#pragma once
/*
*
* started using boost::program_options but decided not to use boost
*
* quick and dirty wrapper for something that looks like boost::program_options
*
* This is not getopt compatible.
* This only support long args i.e with--
* This does not support positional args. {so treat as multivalued}
*Does not support platform specific arg parsing i.e windows style
* This supports switches, multiple values which behaves like python nargs = '+' action = 'append'
* python append will return multiple lists for each occurance
* all options(so far) have default values.So non - default / (required)options are not supported
*
*I never intended to code this !!but once started decided to have a little fun to warm up.
*
*
*/

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <vector>
#include <map>
#include <algorithm>
#include "util.h"


namespace cmdline {

enum class OptionAttribute {

    Switch = 0x1,
    MultiValued = 0x2,
    StopProcessing = 0x4
};

OptionAttribute operator | (OptionAttribute a1, OptionAttribute a2) {
    return OptionAttribute(util::to_underlying(a1) | util::to_underlying(a2));
}
OptionAttribute operator & (OptionAttribute a1, OptionAttribute a2) {
    return OptionAttribute(util::to_underlying(a1) & util::to_underlying(a2));
}

std::string tolower(const std::string& s) {
    std::string ls{s};
    std::transform(ls.begin(), ls.end(), ls.begin(), [](auto c) {
        return std::tolower(c);
    });
    return ls;
}

class OptionDescription {

  public:

    OptionDescription(const  std::string& opt, const std::string& description, const std::string& defValue, OptionAttribute attr)
        :option_{ opt }, optiondescr_{ description }, defaultValue_{ defValue }, attribs_{ attr } {
    }

    OptionDescription(const OptionDescription&) = default;
    OptionDescription(OptionDescription&&) = default;
    ~OptionDescription() = default;

    bool isSwitch() const {
        return (attribs_ & OptionAttribute::Switch) == OptionAttribute::Switch;
    }

    bool isMultivalued() const {
        return (attribs_ & OptionAttribute::MultiValued) == OptionAttribute::MultiValued;
    }
    bool stopProcessing() const {
        return (attribs_ & OptionAttribute::StopProcessing) == OptionAttribute::StopProcessing;
    }

    friend bool operator < (const OptionDescription& optitem1, const OptionDescription& optitem2) {
        return optitem1.option_ < optitem2.option_;
    }

    const std::string& option() const {
        return option_;
    }
    const std::string& description() const {
        return optiondescr_;
    }
    const std::string& defaultValue() const {
        return	defaultValue_;
    }
    bool hasDefault() const {
        return !defaultValue_.empty();
    }

  private:

    const std::string		option_;
    const std::string		optiondescr_;
    const std::string		defaultValue_;
    const OptionAttribute	attribs_{};
};

class OptionsDescription {

    struct EasyInit;

  public:

    OptionsDescription() = default;
    OptionsDescription(const OptionsDescription&) = default;
    ~OptionsDescription() = default;

    EasyInit addOptions() {
        return EasyInit(*this);
    }

    bool contains(const std::string& optname) const {
        return optionDescrs_.count(optname) > 0;
        //need to implement prefix matchingreturn optionDescrs_.lower_bound(optname) != optionDescrs_.end();
    }

    OptionDescription& get(const std::string& optname) {
        auto it = optionDescrs_.lower_bound(optname);
        if (it == optionDescrs_.end())
            throw std::invalid_argument("invalid option " + optname);
        return it->second;
    }

    template<typename F>
    void for_each(F f) const {
        std::for_each(optionDescrs_.begin(), optionDescrs_.end(), [&](auto& it) {
            f(it.second);
        });
    }

    //TODO:make sure option prefix is unique

  private:


    using OptionDescriptions = std::map<std::string, OptionDescription>;

    struct EasyInit {
        EasyInit(OptionsDescription& optionsDescr) :
            optionsDescr_(optionsDescr) {}

        template<typename T>
        EasyInit& operator()(const char* opt, const char* desc, T&& defValue, OptionAttribute attr = {})
        requires(std::is_same_v<std::remove_cvref_t<decltype(std::cout << std::declval<const T&>())>, decltype(std::cout)>) {
            optionsDescr_.add_option(opt, desc, defValue, attr);
            return *this;
        }

        OptionsDescription& optionsDescr_;
    };

    template<typename T>
    void add_option(const char* opt, const char* desc, T&& defaultValue, OptionAttribute attr) {

        std::stringstream ss;
        ss << defaultValue;
        OptionDescription optdesc(opt, desc, ss.str(), attr);

        assert(optionDescrs_.count(opt) == 0);
        optionDescrs_.insert({ tolower(opt),optdesc });
    }


    OptionDescriptions optionDescrs_;

    friend std::ostream& operator << (std::ostream& os, const OptionsDescription& ods) {
        std::cout << "Options:" << std::endl;
        std::for_each(ods.optionDescrs_.begin(), ods.optionDescrs_.end(),
        [&os](auto it) {
            auto& od = it.second;
            os << std::setw(4) << ' '
               << std::setw(12) << od.option() << " "
               << std::setw(24) << od.description();
            auto defValue = od.defaultValue();
            if (!defValue.empty()) {
                os << " " << defValue;
            }
            os << std::endl;

        });
        return os;
    }
};


class Parser {
  public:
    class ArgValue {

      public:
        ArgValue() = default;
        ArgValue(const std::string& value) {
            value_.push_back(value);
        }
        ArgValue(const ArgValue&) = default;
        ArgValue(ArgValue&&) = default;

        ArgValue& operator=(const ArgValue&) = default;
        ArgValue& operator=(ArgValue&&) = default;

        template<typename T>	//no string_view
        T as() const requires(std::is_scalar_v<T> || std::is_same_v<T, std::string>) {
            //TODO: proper processing for bools {on|off|true|false|1|0}
            T t{};
            std::istringstream ss(value_[0]); //single value
            ss >> t;
            return t;
        }

        template<typename T>
        T as() const requires(util::is_std_vector_v<T>)  {
            static_assert(std::is_scalar_v<typename T::value_type> || std::is_same_v<std::string>);
            assert(false); //not yet implemented
            return T{};
        }

        bool empty() const {
            return value_.empty();
        }

        bool isMultivalued() const {
            return (!empty() && (value_.size() > 1));
        }

        void add(const ArgValue& value) {
            const auto& v = value.value_;
            std::copy(v.begin(), v.end(), std::back_inserter(value_));
        }

        void add(const std::string& value) {
            value_.push_back(value);
        }

        void makeSingleValued() {
            if (value_.size() > 1) {
                auto v = *(value_.rbegin());
                value_.clear();
                value_.push_back(v);
            }
        }


      private:


        using Value = std::string;
        using Values = std::vector<std::string>;


        Values value_;
    };




    struct ArgValues :private std::map<std::string, ArgValue> {
        friend class Parser;
        template<typename T>
        T get(const std::string& key)const {
            auto lckey{ tolower(key) };
            auto it{find(lckey)};
            if (it == end()) {
                throw std::invalid_argument("illegal getopt: option " + key + " is not defined");
            }
            return it->second.template as<T>();


        }

    };

    Parser(const OptionsDescription& od)
        :options_(od) {
    }

    ArgValues parse(int argc, const char* argv[]) {
        return doParse(argc, argv);
    }



  private:
    using Result = std::tuple<std::string, ArgValue>;

    OptionsDescription options_;

    //single option parser
    struct OptionParser {
        static inline const std::string OptionDenoter{"--"};
        static  const char ArgDelimitor{ ';' };

        const int	argc_;
        const char* const* argv_;
        int nextIdx_{ 1 };

        OptionParser(int argc, const char* argv[])
            :argc_{ argc }, argv_(argv) {

        }

        operator bool() const {
            return nextIdx_ < argc_;
        }

        bool isOptionDenoter(const std::string& s) const {
            return (s.size() > 2) && (s.substr(0, 2) == OptionDenoter);
        }


        Result next() {
            assert(nextIdx_ < argc_);
            std::string s{argv_[nextIdx_]};
            if (!isOptionDenoter(s))
                throw std::invalid_argument("invalid cmd line param" + s);

            s = s.substr(OptionDenoter.size());
            auto it{ std::find_if(s.begin(), s.end(), [](char c) {
                return (c == ' ' || c == '=');
            }) };
            auto off{ std::distance(s.begin(), it) };
            auto opt{ tolower(s.substr(0,off)) };
            auto argval{ ArgValue() };


            if (off == s.size()) {
                ++nextIdx_;
                while ((nextIdx_ < argc_) && (!isOptionDenoter(argv_[nextIdx_]))) {
                    argval.add(argv_[nextIdx_]);
                    ++nextIdx_;
                }


            } else {
                assert(s[off] == '=');
                s = s.substr(off + 1);
                argval.add(s);
                ++nextIdx_;

            }
            return Result{ opt,argval };

        }

    };


    inline ArgValues	doParse(int argc, const char* argv[]) {
        ArgValues		argValues;
        OptionParser	argParser(argc, argv);
        bool			stopProcessing{ false };


        while ((!stopProcessing) && (argParser)) {
            auto&& [opt, value] = argParser.next();

            if (!options_.contains(opt)) {
                throw std::invalid_argument("unrecognized option:" + opt);
            }

            auto& od = options_.get(opt);
            stopProcessing = od.stopProcessing();
            if (od.isSwitch()) {
                if (!value.empty()) {
                    throw std::invalid_argument("option " + opt + " is a switch does not expect a value");
                }
                argValues[opt] = ArgValue("1");;
                continue;
            }

            if (od.stopProcessing())
                break;

            //TODO: go fullMonty
            if (!od.isMultivalued()) {
                if (value.isMultivalued()) {
                    value.makeSingleValued();

                }
                argValues[opt] = value;

            } else {
                argValues[opt].add(value);

            }

        }

        options_.for_each([&](const auto& od) {
            std::string opt{tolower(od.option())};
            if ((!od.hasDefault()) || (argValues.count(opt)))
                return;

            argValues[opt] = od.defaultValue();
        });
        return argValues;
    }


};

}//namespace cmdline

