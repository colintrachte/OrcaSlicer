#include "Config.hpp"
#include "LocalesUtils.hpp"
#include "PresetJsonKeys.hpp"
#include "PrintConfig.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/fstream.hpp>

#include "nlohmann/json.hpp"

using namespace nlohmann;

namespace Slic3r {

void ConfigBase::set(const std::string &opt_key, double value, bool create)
{
    ConfigOption *opt = this->option_throw(opt_key, create);
    switch (opt->type()) {
    	case coFloat:  			static_cast<ConfigOptionFloat*>(opt)->value = value; break;
    	case coFloatOrPercent:  static_cast<ConfigOptionFloatOrPercent*>(opt)->value = value; static_cast<ConfigOptionFloatOrPercent*>(opt)->percent = false; break;
        case coString: 			static_cast<ConfigOptionString*>(opt)->value = float_to_string_decimal_point(value); break;
    	default: throw BadOptionTypeException("Configbase::set() - conversion from float not possible");
    }
}

ConfigSubstitutions ConfigBase::load_string_map(std::map<std::string, std::string>& key_values, ForwardCompatibilitySubstitutionRule compatibility_rule)
{
    CNumericLocalesSetter locales_setter;

    ConfigSubstitutionContext substitutions_ctxt(compatibility_rule);
    std::map<std::string, std::string>::iterator it;
    for (it = key_values.begin(); it != key_values.end(); it++) {
        try {
            t_config_option_key opt_key = it->first;
            this->set_deserialize(opt_key, it->second, substitutions_ctxt);
        }
        catch (UnknownOptionException& /* e */) {
            // ignore
        }
    }
    return std::move(substitutions_ctxt.substitutions);
}

//BBS: add json support
ConfigSubstitutions ConfigBase::load(const std::string &file, ForwardCompatibilitySubstitutionRule compatibility_rule)
{
    std::map<std::string, std::string> key_values;
    if (is_gcode_file(file))
        return this->load_from_gcode_file(file, compatibility_rule);
    else if (is_json_file(file)) {
        std::string reason;
        return this->load_from_json(file, compatibility_rule, key_values, reason);
    }
    else {
        BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "unsupported format for config file" << file;
        return ConfigSubstitutions();
        //return this->load_from_ini(file, compatibility_rule);
    }
}

//BBS: add json support
ConfigSubstitutions ConfigBase::load_from_json(const std::string &file, ForwardCompatibilitySubstitutionRule compatibility_rule, std::map<std::string, std::string>& key_values, std::string& reason)
{
    int ret = 0;
    ConfigSubstitutionContext substitutions_ctxt(compatibility_rule);

    ret = load_from_json(file, substitutions_ctxt, true, key_values, reason);
    return std::move(substitutions_ctxt.substitutions);
}

int ConfigBase::load_from_json(const std::string &file, ConfigSubstitutionContext& substitution_context, bool load_inherits_to_config, std::map<std::string, std::string>& key_values, std::string& reason)
{
    json j;
    std::list<std::string> different_settings_append;
    std::string new_support_style;
    std::string is_infill_first;
    std::string get_wall_sequence;
    bool is_project_settings = false;

    CNumericLocalesSetter locales_setter;

    std::function<bool(const json::const_iterator&, const char,const char,const bool,std::string&)> parse_str_arr = [&parse_str_arr](const json::const_iterator& it, const char single_sep,const char array_sep,const bool escape_string_style,std::string& value_str)->bool {
        // must have consistent type name
        std::string consistent_type;
        for (auto iter = it.value().begin(); iter != it.value().end(); ++iter) {
            if (consistent_type.empty())
                consistent_type = iter.value().type_name();
            else {
                if (consistent_type != iter.value().type_name())
                    return false;
            }
        }

        bool first = true;
        for (auto iter = it.value().begin(); iter != it.value().end(); iter++) {
            if (iter.value().is_array()) {
                if (!first)
                    value_str += array_sep;
                else
                    first = false;
                bool success = parse_str_arr(iter, single_sep, array_sep,escape_string_style, value_str);
                if (!success)
                    return false;
            }
            else if (iter.value().is_string()) {
                if (!first)
                    value_str += single_sep;
                else
                    first = false;
                if (!escape_string_style)
                    value_str += iter.value();
                else {
                    value_str += "\"";
                    value_str += escape_string_cstyle(iter.value());
                    value_str += "\"";
                }
            }
            else {
                //should not happen
                return false;
            }
        }
        return true;
        };

    try {
        boost::nowide::ifstream ifs(file);
        ifs >> j;
        ifs.close();

        const ConfigDef* config_def = this->def();
        if (config_def == nullptr) {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ": no config defs!";
            return -1;
        }
        auto deserialize_or_record_unknown = [&](const t_config_option_key &opt_key, const std::string &value) {
            try {
                this->set_deserialize(opt_key, value, substitution_context);
                return true;
            } catch (const UnknownOptionException &) {
                if (std::find(substitution_context.unrecogized_keys.begin(), substitution_context.unrecogized_keys.end(), opt_key) ==
                    substitution_context.unrecogized_keys.end())
                    substitution_context.unrecogized_keys.push_back(opt_key);
                return false;
            }
        };
        //parse the json elements
        for (auto it = j.begin(); it != j.end(); it++) {
            if (boost::iequals(it.key(),BBL_JSON_KEY_VERSION)) {
                key_values.emplace(BBL_JSON_KEY_VERSION, it.value());
            }
            else if (boost::iequals(it.key(), BBL_JSON_KEY_IS_CUSTOM)) {
                //skip it
            }
            else if (boost::iequals(it.key(), BBL_JSON_KEY_NAME)) {
                key_values.emplace(BBL_JSON_KEY_NAME, it.value());
                if (it.value() == "project_settings")
                    is_project_settings = true;
            }
            else if (boost::iequals(it.key(), BBL_JSON_KEY_URL)) {
                key_values.emplace(BBL_JSON_KEY_URL, it.value());
            }
            else if (boost::iequals(it.key(), BBL_JSON_KEY_TYPE)) {
                key_values.emplace(BBL_JSON_KEY_TYPE, it.value());
            }
            else if (boost::iequals(it.key(), BBL_JSON_KEY_SETTING_ID)) {
                key_values.emplace(BBL_JSON_KEY_SETTING_ID, it.value());
            }
            else if (boost::iequals(it.key(), BBL_JSON_KEY_FILAMENT_ID)) {
                key_values.emplace(BBL_JSON_KEY_FILAMENT_ID, it.value());
            }
            else if (boost::iequals(it.key(), BBL_JSON_KEY_FROM)) {
                key_values.emplace(BBL_JSON_KEY_FROM, it.value());
            }
            else if (boost::iequals(it.key(), BBL_JSON_KEY_DESCRIPTION)) {
                key_values.emplace(BBL_JSON_KEY_DESCRIPTION, it.value());
            }
            else if (boost::iequals(it.key(), BBL_JSON_KEY_INSTANTIATION)) {
                key_values.emplace(BBL_JSON_KEY_INSTANTIATION, it.value());
            }
            else if (!load_inherits_to_config && boost::iequals(it.key(), BBL_JSON_KEY_INHERITS)) {
                key_values.emplace(BBL_JSON_KEY_INHERITS, it.value());
            } else if (boost::iequals(it.key(), ORCA_JSON_KEY_RENAMED_FROM)) {
                key_values.emplace(ORCA_JSON_KEY_RENAMED_FROM, it.value());
            } else {
                t_config_option_key opt_key = it.key();
                std::string value_str;

                if (it.value().is_string()) {
                    //bool test1 = (it.key() == std::string("end_gcode"));
                    if (! deserialize_or_record_unknown(opt_key, it.value()))
                        continue;
                    //some logic for special values
                    if (opt_key == "support_type") {
                        //std::string new_value = dynamic_cast<ConfigOptionString*>(this->option(opt_key))->value;
                        if (it.value() == "hybrid(auto)") {
                            different_settings_append.push_back(opt_key);
                            different_settings_append.push_back("support_style");
                            new_support_style = "tree_hybrid";
                        }
                    } else if (opt_key == "wall_infill_order") {
                        //BBS: check wall_infill order to decide if it be different and append to diff_setting_append
                        if (it.value() == "outer wall/inner wall/infill" || it.value() == "infill/outer wall/inner wall" || it.value() == "inner-outer-inner wall/infill" || it.value() == "odd-even wall/infill") {
                            get_wall_sequence = "wall_seq_diff_to_system";
                        }

                        if (it.value() == "infill/outer wall/inner wall" || it.value() == "infill/inner wall/outer wall") {
                            different_settings_append.push_back("is_infill_first");
                            is_infill_first = "true";
                        }
                    }
                }
                else if (it.value().is_array()) {
                    t_config_option_key opt_key_src = opt_key;
                    this->handle_legacy(opt_key, value_str);
                    if (opt_key.empty()) {
                        //BBS: record these options
                        substitution_context.unrecogized_keys.push_back(opt_key_src);
                        continue;
                    }
                    bool valid = true, first = true;
                    const ConfigOptionDef* optdef = config_def->get(opt_key);
                    if (optdef == nullptr) {
                        // If we didn't find an option, look for any other option having this as an alias.
                        for (const auto& opt : config_def->options) {
                            for (const t_config_option_key& opt_key2 : opt.second.aliases) {
                                if (opt_key2 == opt_key) {
                                    opt_key = opt.first;
                                    optdef = &opt.second;
                                    break;
                                }
                            }
                            if (optdef != nullptr)
                                break;
                        }
                    }

                    char single_sep = ',';
                    char array_sep = '#';  // currenty not used
                    bool escape_string_type = false;
                    if (optdef) {
                        switch (optdef->type)
                        {
                        case coStrings:
                            escape_string_type = true;
                            single_sep = ';';
                            break;
                        case coPointsGroups:
                            single_sep = '#';
                            break;
                        default:
                            break;
                        }
                    }

                    // BBS: we only support 2 depth array
                    valid = parse_str_arr(it, single_sep, array_sep,escape_string_type, value_str);
                    if (!valid) {
                        BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ": parse " << file << " error, invalid json array for " << it.key();
                        break;
                    }
                    if (valid)
                        deserialize_or_record_unknown(opt_key, value_str);
                }
                else {
                    //should not happen
                    BOOST_LOG_TRIVIAL(error) << __FUNCTION__<< ": parse "<<file<<" error, invalid json type for " << it.key();
                }
            }
        }
        if (!different_settings_append.empty()) {
            if (!new_support_style.empty()) {
                ConfigOptionEnum<SupportMaterialStyle>* opt = this->option<ConfigOptionEnum<SupportMaterialStyle>>("support_style", true);
                opt->value = smsTreeHybrid;
            }

            if (!is_infill_first.empty()) {
                ConfigOptionBool *opt = this->option<ConfigOptionBool>("is_infill_first", true);
                opt->value = true;
            }

            if (is_project_settings) {
                std::vector<std::string>& different_settings = this->option<ConfigOptionStrings>("different_settings_to_system", true)->values;
                size_t size = different_settings.size();
                if (size == 0) {
                    const auto *filament_ids = this->option<ConfigOptionStrings>("filament_settings_id");
                    size = (filament_ids ? filament_ids->values.size() : 0) + 2;
                    different_settings.resize(size);
                }

                std::vector<bool> is_first(size, false);
                std::vector<std::vector<std::string>> original_diffs(size);
                for (int index = 0; index < size; index++)
                {
                    if (different_settings[index].empty()) {
                        is_first[index] = true;
                    }
                    else {
                        // remove unneeded key
                        if (get_wall_sequence.empty()) {
                            std::string wall_sqe_string = "wall_sequence";
                            int pos=different_settings[index].find(wall_sqe_string);

                            if (pos != different_settings[index].npos) {
                                int erase_len = wall_sqe_string.size();
                                if (pos + erase_len < different_settings[index].size() && different_settings[index][pos + erase_len] == ';')
                                    erase_len++;
                                different_settings[index].erase(pos, erase_len);
                            }

                        }

                        if (different_settings[index].empty()) {
                            is_first[index] = true;
                            continue;
                        }

                        Slic3r::unescape_strings_cstyle(different_settings[index], original_diffs[index]);
                    }
                }

                for (auto diff_key : different_settings_append)
                {
                    //get the index in the group
                    int index = 0;
                    bool need_insert = true;
                    if (diff_key == "support_type")
                        index = 0;
                    else if (diff_key == "support_style")
                        index = 0;
                    else if (diff_key == "is_infill_first")
                        index = 0;

                    //check whether exist firstly
                    if (!original_diffs[index].empty()) {
                        for (int j = 0; j < original_diffs[index].size(); j++) {
                            if (original_diffs[index][j] == diff_key) {
                                need_insert = false;
                                break;
                            }
                        }
                    }
                    if (!need_insert)
                        continue;

                    //insert this key
                    if (!is_first[index])
                        different_settings[index] += ";";
                    else
                        is_first[index] = false;
                    different_settings[index] += diff_key;
                }
            }
        }
        
        // Do legacy conversion on a completely loaded dictionary.
        // Perform composite conversions, for example merging multiple keys into one key.
        this->handle_legacy_composite();
        return 0;
    }
    catch (const std::ifstream::failure &err)  {
        BOOST_LOG_TRIVIAL(error) << __FUNCTION__<< ": parse "<<file<<" got a ifstream error, reason = " << err.what();
        reason = std::string("ifstreamError: ") + err.what();
        //throw ConfigurationError(format("Failed loading configuration file \"%1%\": %2%", file, e.what()));
    }
    catch(nlohmann::detail::parse_error &err) {
        BOOST_LOG_TRIVIAL(error) << __FUNCTION__<< ": parse "<<file<<" got a nlohmann::detail::parse_error, reason = " << err.what();
        reason = std::string("JsonParseError: ") + err.what();
        //throw ConfigurationError(format("Failed loading configuration file \"%1%\": %2%", file, err.what()));
    }
    catch(std::exception &err) {
        BOOST_LOG_TRIVIAL(error) << __FUNCTION__<< ": parse "<<file<<" got a generic exception, reason = " << err.what();
        reason = std::string("std::exception: ") + err.what();
    }
    return -1;
}

void ConfigBase::save(const std::string &file) const
{
    boost::nowide::ofstream c;
    c.open(file, std::ios::out | std::ios::trunc);
    c << "# " << Slic3r::header_slic3r_generated() << std::endl;
    for (const std::string &opt_key : this->keys())
        c << opt_key << " = " << this->opt_serialize(opt_key) << std::endl;
    c.close();

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" <<__LINE__ << boost::format(", saved config to %1%\n")%file;
}

} // namespace Slic3r
