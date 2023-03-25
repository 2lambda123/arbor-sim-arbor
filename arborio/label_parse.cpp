#include <any>
#include <limits>
#include <vector>

#include <arborio/label_parse.hpp>

#include <arbor/arbexcept.hpp>
#include <arbor/common_types.hpp>
#include <arbor/iexpr.hpp>
#include <arbor/morph/locset.hpp>
#include <arbor/morph/region.hpp>
#include <arbor/network.hpp>

#include <arbor/util/expected.hpp>

#include "parse_helpers.hpp"

namespace arborio {

label_parse_error::label_parse_error(const std::string& msg, const arb::src_location& loc):
    arb::arbor_exception(concat("error in label description: ", msg," at :", loc.line, ":", loc.column))
{}


namespace {
struct gid_list {
    gid_list() = default;

    gid_list(cell_gid_type gid) : gids({gid}) {}

    std::vector<cell_gid_type> gids;
};

using eval_map_type= std::unordered_multimap<std::string, evaluator>;

eval_map_type eval_map {
    // Functions that return regions
    {"region-nil", make_call<>(arb::reg::nil,
                "'region-nil' with 0 arguments")},
    {"all", make_call<>(arb::reg::all,
                "'all' with 0 arguments")},
    {"tag", make_call<int>(arb::reg::tagged,
                "'tag' with 1 argment: (tag_id:integer)")},
    {"segment", make_call<int>(arb::reg::segment,
                    "'segment' with 1 argment: (segment_id:integer)")},
    {"branch", make_call<int>(arb::reg::branch,
                   "'branch' with 1 argument: (branch_id:integer)")},
    {"cable", make_call<int, double, double>(arb::reg::cable,
                  "'cable' with 3 arguments: (branch_id:integer prox:real dist:real)")},
    {"region", make_call<std::string>(arb::reg::named,
                   "'region' with 1 argument: (name:string)")},
    {"distal-interval", make_call<arb::locset, double>(arb::reg::distal_interval,
                            "'distal-interval' with 2 arguments: (start:locset extent:real)")},
    {"distal-interval", make_call<arb::locset>(
                            [](arb::locset ls){return arb::reg::distal_interval(std::move(ls), std::numeric_limits<double>::max());},
                            "'distal-interval' with 1 argument: (start:locset)")},
    {"proximal-interval", make_call<arb::locset, double>(arb::reg::proximal_interval,
                              "'proximal-interval' with 2 arguments: (start:locset extent:real)")},
    {"proximal-interval", make_call<arb::locset>(
                              [](arb::locset ls){return arb::reg::proximal_interval(std::move(ls), std::numeric_limits<double>::max());},
                              "'proximal_interval' with 1 argument: (start:locset)")},
    {"complete", make_call<arb::region>(arb::reg::complete,
                     "'complete' with 1 argment: (reg:region)")},
    {"radius-lt", make_call<arb::region, double>(arb::reg::radius_lt,
                      "'radius-lt' with 2 arguments: (reg:region radius:real)")},
    {"radius-le", make_call<arb::region, double>(arb::reg::radius_le,
                      "'radius-le' with 2 arguments: (reg:region radius:real)")},
    {"radius-gt", make_call<arb::region, double>(arb::reg::radius_gt,
                      "'radius-gt' with 2 arguments: (reg:region radius:real)")},
    {"radius-ge", make_call<arb::region, double>(arb::reg::radius_ge,
                      "'radius-ge' with 2 arguments: (reg:region radius:real)")},
    {"z-dist-from-root-lt", make_call<double>(arb::reg::z_dist_from_root_lt,
                                "'z-dist-from-root-lt' with 1 arguments: (distance:real)")},
    {"z-dist-from-root-le", make_call<double>(arb::reg::z_dist_from_root_le,
                                "'z-dist-from-root-le' with 1 arguments: (distance:real)")},
    {"z-dist-from-root-gt", make_call<double>(arb::reg::z_dist_from_root_gt,
                                "'z-dist-from-root-gt' with 1 arguments: (distance:real)")},
    {"z-dist-from-root-ge", make_call<double>(arb::reg::z_dist_from_root_ge,
                                "'z-dist-from-root-ge' with 1 arguments: (distance:real)")},
    {"complement", make_call<arb::region>(arb::complement,
                       "'complement' with 1 argment: (reg:region)")},
    {"difference", make_call<arb::region, arb::region>(arb::difference,
                       "'difference' with 2 argments: (reg:region, reg:region)")},
    {"join", make_fold<arb::region>(static_cast<arb::region(*)(arb::region, arb::region)>(arb::join),
                 "'join' with at least 2 arguments: (region region [...region])")},
    {"intersect", make_fold<arb::region>(static_cast<arb::region(*)(arb::region, arb::region)>(arb::intersect),
                      "'intersect' with at least 2 arguments: (region region [...region])")},

    // Functions that return locsets
    {"locset-nil", make_call<>(arb::ls::nil,
                "'locset-nil' with 0 arguments")},
    {"root", make_call<>(arb::ls::root,
                 "'root' with 0 arguments")},
    {"location", make_call<int, double>([](int bid, double pos){return arb::ls::location(arb::msize_t(bid), pos);},
                     "'location' with 2 arguments: (branch_id:integer position:real)")},
    {"terminal", make_call<>(arb::ls::terminal,
                     "'terminal' with 0 arguments")},
    {"distal", make_call<arb::region>(arb::ls::most_distal,
                   "'distal' with 1 argument: (reg:region)")},
    {"proximal", make_call<arb::region>(arb::ls::most_proximal,
                     "'proximal' with 1 argument: (reg:region)")},
    {"distal-translate", make_call<arb::locset, double>(arb::ls::distal_translate,
                     "'distal-translate' with 2 arguments: (ls:locset distance:real)")},
    {"proximal-translate", make_call<arb::locset, double>(arb::ls::proximal_translate,
                     "'proximal-translate' with 2 arguments: (ls:locset distance:real)")},
    {"uniform", make_call<arb::region, int, int, int>(arb::ls::uniform,
                    "'uniform' with 4 arguments: (reg:region, first:int, last:int, seed:int)")},
    {"on-branches", make_call<double>(arb::ls::on_branches,
                        "'on-branches' with 1 argument: (pos:double)")},
    {"on-components", make_call<double, arb::region>(arb::ls::on_components,
                          "'on-components' with 2 arguments: (pos:double, reg:region)")},
    {"boundary", make_call<arb::region>(arb::ls::boundary,
                     "'boundary' with 1 argument: (reg:region)")},
    {"cboundary", make_call<arb::region>(arb::ls::cboundary,
                      "'cboundary' with 1 argument: (reg:region)")},
    {"segment-boundaries", make_call<>(arb::ls::segment_boundaries,
                               "'segment-boundaries' with 0 arguments")},
    {"support", make_call<arb::locset>(arb::ls::support,
                    "'support' with 1 argument (ls:locset)")},
    {"locset", make_call<std::string>(arb::ls::named,
                   "'locset' with 1 argument: (name:string)")},
    {"restrict", make_call<arb::locset, arb::region>(arb::ls::restrict,
                     "'restrict' with 2 arguments: (ls:locset, reg:region)")},
    {"join", make_fold<arb::locset>(static_cast<arb::locset(*)(arb::locset, arb::locset)>(arb::join),
                 "'join' with at least 2 arguments: (locset locset [...locset])")},
    {"sum", make_fold<arb::locset>(static_cast<arb::locset(*)(arb::locset, arb::locset)>(arb::sum),
                "'sum' with at least 2 arguments: (locset locset [...locset])")},


    // iexpr
    {"iexpr", make_call<std::string>(arb::iexpr::named, "iexpr with 1 argument: (value:string)")},

    {"scalar", make_call<double>(arb::iexpr::scalar, "iexpr with 1 argument: (value:double)")},

    {"pi", make_call<>(arb::iexpr::pi, "iexpr with no argument")},

    {"distance", make_call<double, arb::locset>(static_cast<arb::iexpr(*)(double, arb::locset)>(arb::iexpr::distance),
            "iexpr with 2 arguments: (scale:double, loc:locset)")},
    {"distance", make_call<arb::locset>(static_cast<arb::iexpr(*)(arb::locset)>(arb::iexpr::distance),
            "iexpr with 1 argument: (loc:locset)")},
    {"distance", make_call<double, arb::region>(static_cast<arb::iexpr(*)(double, arb::region)>(arb::iexpr::distance),
            "iexpr with 2 arguments: (scale:double, reg:region)")},
    {"distance", make_call<arb::region>(static_cast<arb::iexpr(*)(arb::region)>(arb::iexpr::distance),
            "iexpr with 1 argument: (reg:region)")},

    {"proximal-distance", make_call<double, arb::locset>(static_cast<arb::iexpr(*)(double, arb::locset)>(arb::iexpr::proximal_distance),
            "iexpr with 2 arguments: (scale:double, loc:locset)")},
    {"proximal-distance", make_call<arb::locset>(static_cast<arb::iexpr(*)(arb::locset)>(arb::iexpr::proximal_distance),
            "iexpr with 1 argument: (loc:locset)")},
    {"proximal-distance", make_call<double, arb::region>(static_cast<arb::iexpr(*)(double, arb::region)>(arb::iexpr::proximal_distance),
            "iexpr with 2 arguments: (scale:double, reg:region)")},
    {"proximal-distance", make_call<arb::region>(static_cast<arb::iexpr(*)(arb::region)>(arb::iexpr::proximal_distance),
            "iexpr with 1 arguments: (reg:region)")},

    {"distal-distance", make_call<double, arb::locset>(static_cast<arb::iexpr(*)(double, arb::locset)>(arb::iexpr::distal_distance),
            "iexpr with 2 arguments: (scale:double, loc:locset)")},
    {"distal-distance", make_call<arb::locset>(static_cast<arb::iexpr(*)(arb::locset)>(arb::iexpr::distal_distance),
            "iexpr with 1 argument: (loc:locset)")},
    {"distal-distance", make_call<double, arb::region>(static_cast<arb::iexpr(*)(double, arb::region)>(arb::iexpr::distal_distance),
            "iexpr with 2 arguments: (scale:double, reg:region)")},
    {"distal-distance", make_call<arb::region>(static_cast<arb::iexpr(*)(arb::region)>(arb::iexpr::distal_distance),
            "iexpr with 1 argument: (reg:region)")},

    {"interpolation", make_call<double, arb::locset, double, locset>(static_cast<arb::iexpr(*)(double, arb::locset, double, arb::locset)>(arb::iexpr::interpolation),
            "iexpr with 4 arguments: (prox_value:double, prox_list:locset, dist_value:double, dist_list:locset)")},
    {"interpolation", make_call<double, arb::region, double, region>(static_cast<arb::iexpr(*)(double, arb::region, double, arb::region)>(arb::iexpr::interpolation),
            "iexpr with 4 arguments: (prox_value:double, prox_list:region, dist_value:double, dist_list:region)")},

    {"radius", make_call<double>(static_cast<arb::iexpr(*)(double)>(arb::iexpr::radius), "iexpr with 1 argument: (value:double)")},
    {"radius", make_call<>(static_cast<arb::iexpr(*)()>(arb::iexpr::radius), "iexpr with no argument")},

    {"diameter", make_call<double>(static_cast<arb::iexpr(*)(double)>(arb::iexpr::diameter), "iexpr with 1 argument: (value:double)")},
    {"diameter", make_call<>(static_cast<arb::iexpr(*)()>(arb::iexpr::diameter), "iexpr with no argument")},

    {"exp", make_call<arb::iexpr>(arb::iexpr::exp, "iexpr with 1 argument: (value:iexpr)")},
    {"exp", make_call<double>(arb::iexpr::exp, "iexpr with 1 argument: (value:double)")},

    {"step_right", make_call<arb::iexpr>(arb::iexpr::step_right, "iexpr with 1 argument: (value:iexpr)")},
    {"step_right", make_call<double>(arb::iexpr::step_right, "iexpr with 1 argument: (value:double)")},

    {"step_left", make_call<arb::iexpr>(arb::iexpr::step_left, "iexpr with 1 argument: (value:iexpr)")},
    {"step_left", make_call<double>(arb::iexpr::step_left, "iexpr with 1 argument: (value:double)")},

    {"step", make_call<arb::iexpr>(arb::iexpr::step, "iexpr with 1 argument: (value:iexpr)")},
    {"step", make_call<double>(arb::iexpr::step, "iexpr with 1 argument: (value:double)")},

    {"log", make_call<arb::iexpr>(arb::iexpr::log, "iexpr with 1 argument: (value:iexpr)")},
    {"log", make_call<double>(arb::iexpr::log, "iexpr with 1 argument: (value:double)")},

    {"add", make_conversion_fold<arb::iexpr, arb::iexpr, double>(arb::iexpr::add, "iexpr with at least 2 arguments: ((iexpr | double) (iexpr | double) [...(iexpr | double)])")},

    {"sub", make_conversion_fold<arb::iexpr, arb::iexpr, double>(arb::iexpr::sub, "iexpr with at least 2 arguments: ((iexpr | double) (iexpr | double) [...(iexpr | double)])")},

    {"mul", make_conversion_fold<arb::iexpr, arb::iexpr, double>(arb::iexpr::mul, "iexpr with at least 2 arguments: ((iexpr | double) (iexpr | double) [...(iexpr | double)])")},

    {"div", make_conversion_fold<arb::iexpr, arb::iexpr, double>(arb::iexpr::div, "iexpr with at least 2 arguments: ((iexpr | double) (iexpr | double) [...(iexpr | double)])")},
};

eval_map_type network_eval_map{
    // cell kind
    {"cable-cell", make_call<>([]() { return arb::cell_kind::cable; }, "Cable cell kind")},
    {"lif-cell", make_call<>([]() { return arb::cell_kind::lif; }, "Lif cell kind")},
    {"benchmark-cell",
        make_call<>([]() { return arb::cell_kind::benchmark; }, "Benchmark cell kind")},
    {"spike-source-cell",
        make_call<>([]() { return arb::cell_kind::benchmark; }, "Spike source cell kind")},

    // gid list
    {"gid-list",
        make_call<cell_gid_type>([](cell_gid_type gid) { return gid_list(gid); },
            "List of global indices")},
    {"gid-list",
        make_conversion_fold<gid_list, gid_list, cell_gid_type>(
            [](gid_list a, gid_list b) {
                a.gids.insert(a.gids.end(), b.gids.begin(), b.gids.end());
                return a;
            },
            "List of global indices with at least 2 arguments: ((gid-list | integer) (gid-list | "
            "integer) [...(gid-list | "
            "integer)])")},

    // network_selection
    {"all", make_call<>(arb::network_selection::all, "network selection of all cells and labels")},
    {"none", make_call<>(arb::network_selection::none, "network selection of no cells and labels")},
    {"inter-cell",
        make_call<>(arb::network_selection::inter_cell,
            "network selection of inter-cell connections only")},
    {"network-selection",
        make_call<std::string>(arb::network_selection::named,
            "network selection with 1 argument: (value:string)")},
    {"intersect",
        make_fold<arb::network_selection>(arb::network_selection::intersect,
            "intersection of network selections with at least 2 arguments: "
            "(network_selection network_selection [...network_selection])")},
    {"join",
        make_fold<arb::network_selection>(arb::network_selection::join,
            "join or union operation of network selections with at least 2 arguments: "
            "(network_selection network_selection [...network_selection])")},
    {"symmetric_difference",
        make_fold<arb::network_selection>(arb::network_selection::symmetric_difference,
            "symmetric difference operation between network selections with at least 2 arguments: "
            "(network_selection network_selection [...network_selection])")},
    {"difference",
        make_call<arb::network_selection, arb::network_selection>(
            arb::network_selection::difference,
            "difference of first selection with the second one: "
            "(network_selection network_selection)")},
    {"complement",
        make_call<arb::network_selection>(arb::network_selection::complement,
            "complement of given selection argument: (network_selection)")},
    {"source-cell-kind",
        make_call<arb::cell_kind>(arb::network_selection::source_cell_kind,
            "all sources of cells matching given cell kind argument: (kind:cell-kind)")},
    {"destination-cell-kind",
        make_call<arb::cell_kind>(arb::network_selection::destination_cell_kind,
            "all destinations of cells matching given cell kind argument: (kind:cell-kind)")},
    {"source-gid",
        make_call<cell_gid_type>(
            [](cell_gid_type gid) {
                return arb::network_selection::source_gid(std::vector<cell_gid_type>({gid}));
            },
            "all sources in cell with given gid: (gid:integer)")},
    {"source-gid",
        make_call<gid_list>(
            [](gid_list list) { return arb::network_selection::source_gid(std::move(list.gids)); },
            "all sources of cells gid in list argument: (list: gid-list)")},
    {"destination-gid",
        make_call<cell_gid_type>(
            [](cell_gid_type gid) {
                return arb::network_selection::destination_gid(std::vector<cell_gid_type>({gid}));
            },
            "all destinations in cell with given gid: (gid:integer)")},
    {"destination-gid",
        make_call<gid_list>(
            [](gid_list list) {
                return arb::network_selection::destination_gid(std::move(list.gids));
            },
            "all destinations of cells gid in list argument: (list: gid-list)")},
    {"random-bernoulli",
        make_call<int, double>(arb::network_selection::random_bernoulli,
            "randomly selected with given seed and probability. 2 arguments: (seed:integer, "
            "p:real)")},
    {"random-linear-distance",
        make_call<int, double, double, double, double>(
            arb::network_selection::random_linear_distance,
            "randomly selected with a probability linearly interpolated between [p_begin, p_end] "
            "based on the distance in the interval [distance_begin, distance_end]. 5 arguments: "
            "(seed:integer, distance_begin:real, p_begin:real, distance_end:real, p_end:real)")},
    {"distance-lt",
        make_call<double>(arb::network_selection::distance_lt,
            "Select if distance between source and destination is less than given distance in "
            "micro meter: (distance:real)")},
    {"distance-gt",
        make_call<double>(arb::network_selection::distance_gt,
            "Select if distance between source and destination is greater than given distance in "
            "micro meter: (distance:real)")},

    // network_value
    {"scalar",
        make_call<double>(arb::network_value::scalar,
            "network value with 1 argument: (value:double)")},
    {"network-value",
        make_call<std::string>(arb::network_value::named,
            "network value with 1 argument: (value:string)")},

};

parse_label_hopefully<std::any> eval(const s_expr& e, const eval_map_type& map);

parse_label_hopefully<std::vector<std::any>> eval_args(const s_expr& e, const eval_map_type& map) {
    if (!e) return {std::vector<std::any>{}}; // empty argument list
    std::vector<std::any> args;
    for (auto& h: e) {
        if (auto arg=eval(h, map)) {
            args.push_back(std::move(*arg));
        }
        else {
            return util::unexpected(std::move(arg.error()));
        }
    }
    return args;
}

// Generate a string description of a function evaluation of the form:
// Example output:
//  'foo' with 1 argument: (real)
//  'bar' with 0 arguments
//  'cat' with 3 arguments: (locset region integer)
// Where 'foo', 'bar' and 'cat' are the name of the function, and the
// types (integer, real, region, locset) are inferred from the arguments.
std::string eval_description(const char* name, const std::vector<std::any>& args) {
    auto type_string = [](const std::type_info& t) -> const char* {
        if (t==typeid(int))         return "integer";
        if (t==typeid(double))      return "real";
        if (t==typeid(arb::region)) return "region";
        if (t==typeid(arb::locset)) return "locset";
        return "unknown";
    };

    const auto nargs = args.size();
    std::string msg = concat("'", name, "' with ", nargs, "argument", nargs!=1u?"s:" : ":");
    if (nargs) {
        msg += " (";
        bool first = true;
        for (auto& a: args) {
            msg += concat(first?"":" ", type_string(a.type()));
            first = false;
        }
        msg += ")";
    }
    return msg;
}

// Evaluate an s expression.
// On success the result is wrapped in std::any, where the result is one of:
//      int         : an integer atom
//      double      : a real atom
//      std::string : a string atom: to be treated as a label
//      arb::region : a region
//      arb::locset : a locset
//
// If there invalid input is detected, hopefully return value contains
// a label_error_state with an error string and location.
//
// If there was an unexpected/fatal error, an exception will be thrown.
parse_label_hopefully<std::any> eval(const s_expr& e, const eval_map_type& map) {
    if (e.is_atom()) {
        return eval_atom<label_parse_error>(e);
    }
    if (e.head().is_atom()) {
        // This must be a function evaluation, where head is the function name, and
        // tail is a list of arguments.

        // Evaluate the arguments, and return error state if an error occurred.
        auto args = eval_args(e.tail(), map);
        if (!args) {
            return util::unexpected(args.error());
        }

        // Find all candidate functions that match the name of the function.
        auto& name = e.head().atom().spelling;
        auto matches = map.equal_range(name);
        // Search for a candidate that matches the argument list.
        for (auto i=matches.first; i!=matches.second; ++i) {
            if (i->second.match_args(*args)) { // found a match: evaluate and return.
                return i->second.eval(*args);
            }
        }

        // Unable to find a match: try to return a helpful error message.
        const auto nc = std::distance(matches.first, matches.second);
        auto msg = concat("No matches for ", eval_description(name.c_str(), *args), "\n  There are ", nc, " potential candidates", nc?":":".");
        int count = 0;
        for (auto i=matches.first; i!=matches.second; ++i) {
            msg += concat("\n  Candidate ", ++count, "  ", i->second.message);
        }
        return util::unexpected(label_parse_error(msg, location(e)));
    }

    return util::unexpected(label_parse_error(
                                concat("'", e, "' is not either integer, real expression of the form (op <args>)"),
                                location(e)));
}

} // namespace

ARB_ARBORIO_API parse_label_hopefully<std::any> parse_label_expression(const std::string& e) {
    return eval(parse_s_expr(e), eval_map);
}
ARB_ARBORIO_API parse_label_hopefully<std::any> parse_label_expression(const s_expr& s) {
    return eval(s, eval_map);
}

ARB_ARBORIO_API parse_label_hopefully<arb::region> parse_region_expression(const std::string& s) {
    if (auto e = eval(parse_s_expr(s), eval_map)) {
        if (e->type() == typeid(region)) {
            return {std::move(std::any_cast<region&>(*e))};
        }
        if (e->type() == typeid(std::string)) {
            return {reg::named(std::move(std::any_cast<std::string&>(*e)))};
        }
        return util::unexpected(
                label_parse_error(
                    concat("Invalid region description: '", s ,"' is neither a valid region expression or region label string.")));
    }
    else {
        return util::unexpected(label_parse_error(std::string()+e.error().what()));
    }
}

ARB_ARBORIO_API parse_label_hopefully<arb::locset> parse_locset_expression(const std::string& s) {
    if (auto e = eval(parse_s_expr(s), eval_map)) {
        if (e->type() == typeid(locset)) {
            return {std::move(std::any_cast<locset&>(*e))};
        }
        if (e->type() == typeid(std::string)) {
            return {ls::named(std::move(std::any_cast<std::string&>(*e)))};
        }
        return util::unexpected(
            label_parse_error(
                    concat("Invalid region description: '", s ,"' is neither a valid locset expression or locset label string.")));
    }
    else {
        return util::unexpected(label_parse_error(std::string()+e.error().what()));
    }
}

parse_label_hopefully<arb::iexpr> parse_iexpr_expression(const std::string& s) {
    if (auto e = eval(parse_s_expr(s), eval_map)) {
        if (e->type() == typeid(iexpr)) {
            return {std::move(std::any_cast<iexpr&>(*e))};
        }
        return util::unexpected(
                label_parse_error(
                    concat("Invalid iexpr description: '", s)));
    }
    else {
        return util::unexpected(label_parse_error(std::string()+e.error().what()));
    }
}


parse_label_hopefully<arb::network_selection> parse_network_selection_expression(const std::string& s) {
    if (auto e = eval(parse_s_expr(s), network_eval_map)) {
        if (e->type() == typeid(arb::network_selection)) {
            return {std::move(std::any_cast<arb::network_selection&>(*e))};
        }
        return util::unexpected(
                label_parse_error(
                    concat("Invalid iexpr description: '", s)));
    }
    else {
        return util::unexpected(label_parse_error(std::string()+e.error().what()));
    }
}

parse_label_hopefully<arb::network_value> parse_network_value_expression(const std::string& s) {
    if (auto e = eval(parse_s_expr(s), network_eval_map)) {
        if (e->type() == typeid(arb::network_value)) {
            return {std::move(std::any_cast<arb::network_value&>(*e))};
        }
        return util::unexpected(
                label_parse_error(
                    concat("Invalid iexpr description: '", s)));
    }
    else {
        return util::unexpected(label_parse_error(std::string()+e.error().what()));
    }
}

} // namespace arborio
