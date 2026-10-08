#include <catch2/catch_test_macros.hpp>
#include <discusy/events.hpp>
#include <discusy/json.hpp>
#include <discusy/etf.hpp>
#include <etf/etf.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace test_component_types {

struct AltIR {
    std::uint8_t component_type{3};
    std::string custom_id{"alt_custom"};
    std::vector<std::string> values{"alpha", "beta"};
};

struct InvalidComp {
    std::uint8_t type{250};
    std::string custom_id{"bad"};
};

} // namespace test_component_types

TEST_CASE("Components: JSON serialization and deserialization", "[components][json]") {
    SECTION("Button inside action_row_component roundtrip") {
        auto btn = discusy::components::Button::create(
            discusy::components::button_style::Primary,
            "Click Me",
            "btn_1"
        );
        discusy::components::action_row_component arc = btn;

        std::string json_str;
        REQUIRE_FALSE(discusy::json::write(arc, json_str));
        CHECK(json_str.contains("\"type\":2"));
        CHECK(json_str.contains("\"label\":\"Click Me\""));
        CHECK(json_str.contains("\"custom_id\":\"btn_1\""));

        discusy::components::action_row_component decoded_arc;
        REQUIRE_FALSE(discusy::json::parse(decoded_arc, json_str));
        REQUIRE(decoded_arc.is<discusy::components::Button>());

        const auto* decoded_btn = decoded_arc.get_if<discusy::components::Button>();
        REQUIRE(decoded_btn != nullptr);
        CHECK(decoded_btn->style == discusy::components::button_style::Primary);
        CHECK(decoded_btn->label == "Click Me");
        CHECK(decoded_btn->custom_id == "btn_1");
    }

    SECTION("ActionRow inside component with multiple children roundtrip") {
        discusy::components::ActionRow row{};
        row.add_button(discusy::components::Button::create(
            discusy::components::button_style::Success, "OK", "btn_ok"));
        row.add_string_select(discusy::components::StringSelect::create("select_1"));

        discusy::components::component comp = row;
        std::string json_str;
        REQUIRE_FALSE(discusy::json::write(comp, json_str));

        discusy::components::component comp_decoded;
        REQUIRE_FALSE(discusy::json::parse(comp_decoded, json_str));
        REQUIRE(comp_decoded.is<discusy::components::ActionRow>());

        const auto* decoded_row = comp_decoded.get_if<discusy::components::ActionRow>();
        REQUIRE(decoded_row != nullptr);
        REQUIRE(decoded_row->components.size() == 2);
        CHECK(decoded_row->components[0].is<discusy::components::Button>());
        CHECK(decoded_row->components[1].is<discusy::components::StringSelect>());

        const auto* btn = decoded_row->components[0].get_if<discusy::components::Button>();
        REQUIRE(btn != nullptr);
        CHECK(btn->label == "OK");
        CHECK(btn->custom_id == "btn_ok");

        const auto* sel = decoded_row->components[1].get_if<discusy::components::StringSelect>();
        REQUIRE(sel != nullptr);
        CHECK(sel->custom_id == "select_1");
    }

    SECTION("Parsing raw Discord JSON payload into action_row_component") {
        std::string raw_json = R"({"type":2,"style":1,"label":"Submit","custom_id":"submit_btn"})";

        discusy::components::action_row_component arc;
        REQUIRE_FALSE(discusy::json::parse(arc, raw_json));
        REQUIRE(arc.is<discusy::components::Button>());

        const auto* btn = arc.get_if<discusy::components::Button>();
        REQUIRE(btn != nullptr);
        CHECK(btn->style == discusy::components::button_style::Primary);
        CHECK(btn->label == "Submit");
        CHECK(btn->custom_id == "submit_btn");
    }

    SECTION("Parsing raw Discord JSON ActionRow with mixed buttons and selects") {
        std::string raw_json = R"({"type":1,"components":[{"type":2,"style":2,"label":"Cancel","custom_id":"cancel_btn"},{"type":3,"custom_id":"menu_1","placeholder":"Choose an option"}]})";

        discusy::components::component comp;
        REQUIRE_FALSE(discusy::json::parse(comp, raw_json));
        REQUIRE(comp.is<discusy::components::ActionRow>());

        const auto* row = comp.get_if<discusy::components::ActionRow>();
        REQUIRE(row != nullptr);
        REQUIRE(row->components.size() == 2);
        CHECK(row->components[0].is<discusy::components::Button>());
        CHECK(row->components[1].is<discusy::components::StringSelect>());

        const auto* btn = row->components[0].get_if<discusy::components::Button>();
        REQUIRE(btn != nullptr);
        CHECK(btn->label == "Cancel");
        CHECK(btn->custom_id == "cancel_btn");

        const auto* sel = row->components[1].get_if<discusy::components::StringSelect>();
        REQUIRE(sel != nullptr);
        CHECK(sel->custom_id == "menu_1");
        CHECK(sel->placeholder == "Choose an option");
    }

    SECTION("Component interaction response with type field (JSON)") {
        std::string raw_json = R"({"type":3,"id":1,"custom_id":"my_select","values":["apple","banana"]})";

        discusy::interaction::component_interaction_response cir;
        REQUIRE_FALSE(discusy::json::parse(cir, raw_json));
        REQUIRE(cir.is<discusy::components::StringSelectInteractionResponse>());

        const auto* ir = cir.get_if<discusy::components::StringSelectInteractionResponse>();
        REQUIRE(ir != nullptr);
        CHECK(ir->custom_id == "my_select");
        REQUIRE(ir->values.size() == 2);
        CHECK(ir->values[0] == "apple");
        CHECK(ir->values[1] == "banana");
    }

    SECTION("Component interaction response with component_type fallback (JSON)") {
        std::string raw_json = R"({"component_type":3,"id":2,"custom_id":"alt_select","values":["valA"]})";

        discusy::interaction::component_interaction_response cir;
        REQUIRE_FALSE(discusy::json::parse(cir, raw_json));
        REQUIRE(cir.is<discusy::components::StringSelectInteractionResponse>());

        const auto* ir = cir.get_if<discusy::components::StringSelectInteractionResponse>();
        REQUIRE(ir != nullptr);
        CHECK(ir->custom_id == "alt_select");
        REQUIRE(ir->values.size() == 1);
        CHECK(ir->values[0] == "valA");
    }

    SECTION("Unknown component type returns error (JSON)") {
        std::string raw_json = R"({"type":999,"custom_id":"bad"})";

        discusy::components::action_row_component arc;
        CHECK(discusy::json::parse(arc, raw_json) == true);
    }
}

TEST_CASE("Components: ETF serialization and deserialization", "[components][etf]") {
    SECTION("Button inside action_row_component roundtrip") {
        auto btn = discusy::components::Button::create(
            discusy::components::button_style::Primary,
            "Click Me",
            "btn_1"
        );
        discusy::components::action_row_component arc = btn;

        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(arc, encoded));
        REQUIRE(encoded.size() > 5);

        discusy::components::action_row_component decoded_arc;
        REQUIRE_FALSE(glz::read_etf(decoded_arc, encoded));
        REQUIRE(decoded_arc.is<discusy::components::Button>());

        const auto* decoded_btn = decoded_arc.get_if<discusy::components::Button>();
        REQUIRE(decoded_btn != nullptr);
        CHECK(decoded_btn->style == discusy::components::button_style::Primary);
        CHECK(decoded_btn->label == "Click Me");
        CHECK(decoded_btn->custom_id == "btn_1");
    }

    SECTION("ActionRow inside component with multiple children roundtrip") {
        discusy::components::ActionRow row{};
        row.add_button(discusy::components::Button::create(
            discusy::components::button_style::Success, "OK", "btn_ok"));
        row.add_string_select(discusy::components::StringSelect::create("select_1"));

        discusy::components::component comp = row;
        std::string comp_encoded;
        REQUIRE_FALSE(glz::write_etf(comp, comp_encoded));

        discusy::components::component comp_decoded;
        REQUIRE_FALSE(glz::read_etf(comp_decoded, comp_encoded));
        REQUIRE(comp_decoded.is<discusy::components::ActionRow>());

        const auto* decoded_row = comp_decoded.get_if<discusy::components::ActionRow>();
        REQUIRE(decoded_row != nullptr);
        REQUIRE(decoded_row->components.size() == 2);
        CHECK(decoded_row->components[0].is<discusy::components::Button>());
        CHECK(decoded_row->components[1].is<discusy::components::StringSelect>());

        const auto* btn = decoded_row->components[0].get_if<discusy::components::Button>();
        REQUIRE(btn != nullptr);
        CHECK(btn->label == "OK");
        CHECK(btn->custom_id == "btn_ok");

        const auto* sel = decoded_row->components[1].get_if<discusy::components::StringSelect>();
        REQUIRE(sel != nullptr);
        CHECK(sel->custom_id == "select_1");
    }

    SECTION("Component interaction response with type field (ETF)") {
        auto ir = discusy::components::StringSelectInteractionResponse::create(
            1, "my_select", discusy::make_vector<std::string>("val1", "val2"));
        discusy::interaction::component_interaction_response cir = ir;

        std::string cir_encoded;
        REQUIRE_FALSE(glz::write_etf(cir, cir_encoded));

        discusy::interaction::component_interaction_response cir_decoded;
        REQUIRE_FALSE(glz::read_etf(cir_decoded, cir_encoded));
        REQUIRE(cir_decoded.is<discusy::components::StringSelectInteractionResponse>());

        const auto* decoded_ir = cir_decoded.get_if<discusy::components::StringSelectInteractionResponse>();
        REQUIRE(decoded_ir != nullptr);
        CHECK(decoded_ir->custom_id == "my_select");
        REQUIRE(decoded_ir->values.size() == 2);
        CHECK(decoded_ir->values[0] == "val1");
        CHECK(decoded_ir->values[1] == "val2");
    }

    SECTION("Component interaction response with component_type fallback (ETF)") {
        test_component_types::AltIR alt{};
        std::string alt_encoded;
        REQUIRE_FALSE(glz::write_etf(alt, alt_encoded));

        discusy::interaction::component_interaction_response alt_decoded;
        REQUIRE_FALSE(glz::read_etf(alt_decoded, alt_encoded));
        REQUIRE(alt_decoded.is<discusy::components::StringSelectInteractionResponse>());

        const auto* alt_ir = alt_decoded.get_if<discusy::components::StringSelectInteractionResponse>();
        REQUIRE(alt_ir != nullptr);
        CHECK(alt_ir->custom_id == "alt_custom");
        REQUIRE(alt_ir->values.size() == 2);
        CHECK(alt_ir->values[0] == "alpha");
        CHECK(alt_ir->values[1] == "beta");
    }

    SECTION("Unknown component type returns error (ETF)") {
        test_component_types::InvalidComp inv{};
        std::string inv_encoded;
        REQUIRE_FALSE(glz::write_etf(inv, inv_encoded));

        discusy::components::action_row_component bad_arc;
        auto ec = glz::read_etf(bad_arc, inv_encoded);
        CHECK(bool(ec));
    }
}
