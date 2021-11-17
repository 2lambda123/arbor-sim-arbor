#include <ostream>
#include <set>
#include <string>
#include <regex>

#define FMT_HEADER_ONLY YES
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/compile.h>

#include "blocks.hpp"
#include "infoprinter.hpp"
#include "module.hpp"
#include "printerutil.hpp"

#include "io/ostream_wrappers.hpp"
#include "io/prefixbuf.hpp"

using io::quote;

std::string build_info_header(const Module& m, const printer_options& opt, bool cpu, bool gpu) {
    using io::indent;
    using io::popindent;

    std::string name = m.module_name();

    io::pfxstringstream out;

    std::string fingerprint = "<placeholder>";

    const auto lowest = std::to_string(std::numeric_limits<double>::lowest());
    const auto max    = std::to_string(std::numeric_limits<double>::max());
    out << fmt::format("#pragma once\n\n"
                       "#include <cmath>\n"
                       "#include <{}mechanism_abi.h>\n\n",
                       arb_header_prefix());

    out << fmt::format("extern \"C\" {{\n"
                       "  arb_mechanism_type make_{0}_{1}() {{\n"
                       "    // Tables\n",
                       std::regex_replace(opt.cpp_namespace, std::regex{"::"}, "_"),
                       name);

    const auto& [state_ids, global_ids, param_ids] = public_variable_ids(m);
    const auto& assigned_ids = m.assigned_block().parameters;
    auto fmt_id = [&](const auto& id) {
        auto lo  = id.has_range() ? id.range.first  : lowest;
        auto hi  = id.has_range() ? id.range.second : max;
        auto val = id.has_value() ? id.value        : "NAN";
        return fmt::format(FMT_COMPILE("{{ \"{}\", \"{}\", {}, {}, {} }}"), id.name(),
                id.unit_string(), val, lo, hi);
    };
    auto fmt_ion = [&](const auto& ion) {
        return fmt::format(FMT_COMPILE("{{ \"{}\", {}, {}, {}, {}, {}, {}, {} }}"),
           ion.name,
           ion.writes_concentration_int(), ion.writes_concentration_ext(),
           ion.writes_rev_potential(), ion.uses_rev_potential(),
           ion.uses_valence(), ion.verifies_valence(), ion.expected_valence);
    };
    auto print_array_head = [&](char const * type, char const * name, auto size) {
        out << "    static " << type;
        if (size) out << " " << name << "[] = {";
        else out << "* " << name << " = NULL;";
    };
    auto print_array_tail = [&](char const * type, char const * name, auto size) {
        if (size) out << " };";
        out << fmt::format("\n    static arb_size_type n_{} = {};\n", name, size);
    };
    auto print_array = [&](char const* type, char const * name, auto & fmt_func, auto const & arr) {
        io::separator sep("", ",\n        ");
        print_array_head(type, name, arr.size());
        for (const auto& var: arr) out << sep << fmt_func(var);
        print_array_tail(type, name, arr.size());
    };
    auto print_arrays = [&](char const* type, char const * name, auto & fmt_func, auto const & arr0, auto const & arr1) {
        io::separator sep("", ",\n        ");
        print_array_head(type, name, arr0.size() + arr1.size());
        for (const auto& var: arr0) out << sep << fmt_func(var);
        for (const auto& var: arr1) out << sep << fmt_func(var);
        print_array_tail(type, name, arr0.size() + arr1.size());
    };

    print_array("arb_field_info", "globals", fmt_id, global_ids);
    print_arrays("arb_field_info", "state_vars", fmt_id, state_ids, assigned_ids);
    print_array("arb_field_info", "parameters", fmt_id, param_ids);
    print_array("arb_ion_info", "ions", fmt_ion, m.ion_deps());

    out << fmt::format(FMT_COMPILE("\n"
                                   "    arb_mechanism_type result;\n"
                                   "    result.abi_version=ARB_MECH_ABI_VERSION;\n"
                                   "    result.fingerprint=\"{1}\";\n"
                                   "    result.name=\"{0}\";\n"
                                   "    result.kind={2};\n"
                                   "    result.is_linear={3};\n"
                                   "    result.has_post_events={4};\n"
                                   "    result.globals=globals;\n"
                                   "    result.n_globals=n_globals;\n"
                                   "    result.ions=ions;\n"
                                   "    result.n_ions=n_ions;\n"
                                   "    result.state_vars=state_vars;\n"
                                   "    result.n_state_vars=n_state_vars;\n"
                                   "    result.parameters=parameters;\n"
                                   "    result.n_parameters=n_parameters;\n"
                                   "    return result;\n"
                                   "  }}\n"
                                   "\n"),
                       name,
                       fingerprint,
                       module_kind_str(m),
                       m.is_linear(),
                       m.has_post_events())
        << fmt::format("  arb_mechanism_interface* make_{0}_{1}_interface_multicore(){2}\n"
                       "  arb_mechanism_interface* make_{0}_{1}_interface_gpu(){3}\n"
                       "}}\n",
                       std::regex_replace(opt.cpp_namespace, std::regex{"::"}, "_"),
                       name,
                       cpu ? ";" : " { return nullptr; }",
                       gpu ? ";" : " { return nullptr; }");
    return out.str();
}
