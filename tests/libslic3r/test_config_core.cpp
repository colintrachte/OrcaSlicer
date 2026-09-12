#include <catch2/catch_all.hpp>

#include "Config.hpp"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {

class FocusedConfigDef : public Slic3r::ConfigDef
{
public:
    FocusedConfigDef()
    {
        auto *count = this->add("count", Slic3r::coInt);
        count->set_default_value(new Slic3r::ConfigOptionInt(3));

        auto *enabled = this->add("enabled", Slic3r::coBool);
        enabled->set_default_value(new Slic3r::ConfigOptionBool(false));

        auto *label = this->add("label", Slic3r::coString);
        label->set_default_value(new Slic3r::ConfigOptionString("default"));

        auto *temperatures = this->add_nullable("temperatures", Slic3r::coInts);
        temperatures->set_default_value(new Slic3r::ConfigOptionIntsNullable({
            Slic3r::ConfigOptionIntsNullable::nil_value(), 200
        }));
    }
};

class FocusedConfig : public Slic3r::DynamicConfig
{
public:
    const Slic3r::ConfigDef *def() const override
    {
        static const FocusedConfigDef definition;
        return &definition;
    }
};

} // namespace

TEST_CASE("Focused config creates and strictly deserializes generic options", "[ConfigCore]")
{
    FocusedConfig config;

    REQUIRE(config.option("count", true) != nullptr);
    CHECK(config.opt_int("count") == 3);

    config.set_deserialize_strict("count", "12");
    CHECK(config.opt_int("count") == 12);
    CHECK_THROWS_AS(config.set_deserialize_strict("count", "invalid"), Slic3r::BadOptionValueException);
}

TEST_CASE("Focused config preserves diff equality and nullable semantics", "[ConfigCore]")
{
    FocusedConfig left;
    FocusedConfig right;

    left.set("count", 7, true);
    right.set("count", 7, true);
    REQUIRE(left.equals(right));
    REQUIRE(left.diff(right).empty());

    right.set("count", 8);
    CHECK_FALSE(left.equals(right));
    CHECK(left.diff(right) == Slic3r::t_config_option_keys{"count"});

    left.set_deserialize_strict("temperatures", "nil,215");
    const auto *temperatures = left.option<Slic3r::ConfigOptionIntsNullable>("temperatures");
    REQUIRE(temperatures != nullptr);
    REQUIRE(temperatures->values.size() == 2);
    CHECK(temperatures->is_nil(0));
    CHECK(temperatures->values[1] == 215);
}

TEST_CASE("Focused config parses command-line options without the core monolith", "[ConfigCore]")
{
    FocusedConfig config;
    const char *argv[] = {"config-test", "--count", "9", "--enabled", "model.stl", "--", "--literal"};
    Slic3r::t_config_option_keys extra;
    Slic3r::t_config_option_keys parsed;

    REQUIRE(config.read_cli(static_cast<int>(std::size(argv)), argv, &extra, &parsed));
    CHECK(config.opt_int("count") == 9);
    CHECK(config.opt_bool("enabled"));
    CHECK(parsed == Slic3r::t_config_option_keys{"count", "enabled"});
    CHECK(extra == Slic3r::t_config_option_keys{"model.stl", "--literal"});
}

TEST_CASE("Focused dynamic config preserves values across copy and move", "[ConfigCore]")
{
    Slic3r::DynamicConfig source;
    source.set_key_value("count", new Slic3r::ConfigOptionInt(11));
    source.set_key_value("label", new Slic3r::ConfigOptionString("standalone"));
    source.set_key_value("temperatures", new Slic3r::ConfigOptionIntsNullable({
        Slic3r::ConfigOptionIntsNullable::nil_value(), 220
    }));

    Slic3r::DynamicConfig copied(source);
    REQUIRE(source == copied);
    REQUIRE(source.diff(copied).empty());

    Slic3r::DynamicConfig moved(std::move(copied));
    CHECK(source == moved);
    CHECK(copied.empty());
}
