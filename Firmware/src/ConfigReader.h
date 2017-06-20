#pragma once

#include <string>
#include <map>
#include <set>
#include <istream>
#include <regex>

class ConfigReader
{
public:
    ConfigReader(std::istream& ist) : is(ist) {};
    ~ConfigReader(){};

    void reset() { is.clear(); is.seekg (0, is.beg); }
    using section_map_t = std::map<std::string, std::string>;
    using sub_section_map_t =  std::map<std::string, section_map_t>;
    using sections_t = std::set<std::string>;
    bool get_sections(sections_t& sections);
    bool get_section(const char *section, section_map_t& config);
    bool get_sub_sections(const char *section, sub_section_map_t& config);

    const std::string& get_current_section() const { return current_section; }

    const std::string get_string(const section_map_t&, const char *key, const char *def="");
    float get_float(const section_map_t&, const char *key, float def=0.0F);
    int get_int(const section_map_t&, const char *key, int def=0);
    bool get_bool(const section_map_t&, const char *key, bool def=false);

private:
    std::istream& is;

    std::string current_section;

    // regular expression to extract section
    std::regex section_test{"\\[(.*?)\\]"};
    // regex to extract the key = value pair and the comment
    std::regex value_test{"([_.[:alnum:]]+)\\s*=\\s*([^#]+)(#.*)?"};
    // regex to extract the key = value pair and the comment
    std::regex sub_value_test{"(\\w+)\\.(\\w+)\\s*=\\s*([^#]+)(#.*)?"};
};
