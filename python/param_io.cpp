#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <fstream>

#include <arbor/cable_cell.hpp>
#include <arbor/morph/primitives.hpp>
#include <sup/json_params.hpp>

#include "error.hpp"

namespace pyarb {
using sup::find_and_remove_json;

arb::cable_cell_parameter_set load_cell_parameters(nlohmann::json& defaults_json) {
    arb::cable_cell_parameter_set defaults;

    defaults.init_membrane_potential = find_and_remove_json<double>("Vm", defaults_json);
    defaults.membrane_capacitance    = find_and_remove_json<double>("cm", defaults_json);
    defaults.axial_resistivity       = find_and_remove_json<double>("Ra", defaults_json);
    auto temp_c = find_and_remove_json<double>("celsius", defaults_json);
    if (temp_c) {
        defaults.temperature_K = temp_c.value() + 273.15;
    }

    if (auto ions_json = find_and_remove_json<nlohmann::json>("ions", defaults_json)) {
        auto ions_map = ions_json.value().get<std::unordered_map<std::string, nlohmann::json>>();
        for (auto& i: ions_map) {
            auto ion_name = i.first;
            auto ion_json = i.second;

            arb::cable_cell_ion_data ion_data;
            if (auto iconc = find_and_remove_json<double>("internal-concentration", ion_json)) {
                ion_data.init_int_concentration = iconc.value();
            }
            if (auto econc = find_and_remove_json<double>("external-concentration", ion_json)) {
                ion_data.init_ext_concentration = econc.value();
            }
            if (auto rev_pot = find_and_remove_json<double>("reversal-potential", ion_json)) {
                ion_data.init_reversal_potential = rev_pot.value();
            }
            defaults.ion_data.insert({ion_name, ion_data});

            if (auto method = find_and_remove_json<std::string>("method", ion_json)) {
                if (method.value() == "nernst") {
                    defaults.reversal_potential_method.insert({ion_name, "nernst/" + ion_name});
                } else if (method.value() != "constant") {
                    throw pyarb_error("method of ion \"" + ion_name + "\" can only be either constant or nernst");
                }
            }
        }
    }
    return defaults;
}

arb::mechanism_desc load_mechanism_desc(nlohmann::json& mech_json) {
    auto name = find_and_remove_json<std::string>("mechanism", mech_json);
    if (name) {
        auto mech = arb::mechanism_desc(name.value());
        auto params = find_and_remove_json<std::unordered_map<std::string, double>>("parameters", mech_json);
        if (params) {
            for (auto p: params.value()) {
                mech.set(p.first, p.second);
            }
        }
        return mech;
    }
    throw pyarb::pyarb_error("Mechanism not specified");
}

// Checks that all the argumnets in the arb::cable_cell_parameter_set are valid and throws an exception if that's not the case
void check_defaults(const arb::cable_cell_parameter_set& defaults) {
    if(!defaults.temperature_K) throw pyarb_error("temperature missing");
    if(!defaults.init_membrane_potential) throw pyarb_error("initial membrane potential missing");
    if(!defaults.axial_resistivity) throw pyarb_error("axial resistivity missing");
    if(!defaults.membrane_capacitance) throw pyarb_error("membrane capacitance missing");

    for (auto ion_data: defaults.ion_data) {
        auto ion = ion_data.first;
        auto& ion_params = ion_data.second;

        if(isnan(ion_params.init_int_concentration))  throw pyarb_error("initial internal concentration of " + ion + " missing");
        if(isnan(ion_params.init_ext_concentration))  throw pyarb_error("initial external concentration of " + ion + " missing");
        if(isnan(ion_params.init_reversal_potential)) throw pyarb_error("initial reversal potential of " + ion + " missing");
    }

    // Check that ca, na and k are initialized
    std::vector<std::string> default_ions = {"ca", "na", "k"};
    for (auto ion:default_ions) {
        if (!defaults.ion_data.count(ion)) throw pyarb_error("initial parameters of " + ion + " missing");
    }
}

arb::cable_cell_parameter_set overwrite_cable_parameters(const arb::cable_cell_parameter_set& base, const arb::cable_cell_parameter_set& overwrite) {
    arb::cable_cell_parameter_set merged = base;
    if (auto temp = overwrite.temperature_K) {
        merged.temperature_K = temp;
    }
    if (auto cm = overwrite.membrane_capacitance) {
        merged.membrane_capacitance = cm;
    }
    if (auto ra = overwrite.axial_resistivity) {
        merged.axial_resistivity = ra;
    }
    if (auto vm = overwrite.init_membrane_potential) {
        merged.init_membrane_potential = vm;
    }
    for (auto ion: overwrite.ion_data) {
        auto name = ion.first;
        auto data = ion.second;
        if (!isnan(data.init_reversal_potential)) {
            merged.ion_data[name].init_reversal_potential = data.init_reversal_potential;
        }
        if (!isnan(data.init_ext_concentration)) {
            merged.ion_data[name].init_ext_concentration = data.init_ext_concentration;
        }
        if (!isnan(data.init_int_concentration)) {
            merged.ion_data[name].init_int_concentration = data.init_int_concentration;
        }
    }
    for (auto ion: overwrite.reversal_potential_method) {
        auto name = ion.first;
        auto data = ion.second;
        merged.reversal_potential_method[name] = data;
    }
    return merged;
}

void output_cell_params(const arb::cable_cell& cell, std::string file_name) {
    // Global
    nlohmann::json json_file, global, ion_data;

    global["celsius"] = cell.default_parameters.temperature_K.value() - 273.15;
    global["Vm"] = cell.default_parameters.init_membrane_potential.value();
    global["Ra"] = cell.default_parameters.axial_resistivity.value();
    global["cm"] = cell.default_parameters.membrane_capacitance.value();
    for (auto ion: cell.default_parameters.ion_data) {
        auto ion_name = ion.first;
        nlohmann::json data;
        data["internal-concentration"] = ion.second.init_int_concentration;
        data["external-concentration"] = ion.second.init_ext_concentration;
        data["reversal-potential"] = ion.second.init_reversal_potential;
        if (cell.default_parameters.reversal_potential_method.count(ion_name)) {
            data["method"] = cell.default_parameters.reversal_potential_method.at(ion_name).name();
        }
        ion_data[ion.first] = data;
    }
    global["ions"] = ion_data;
    json_file["global"] = global;

    // Local
    std::unordered_map<std::string, nlohmann::json> regions;

    for (const auto& entry: cell.get_region_temperatures()) {
        regions[entry.first]["celsius"] = entry.second.value;
    }
    for (const auto& entry: cell.get_region_init_membrabe_potentials()) {
        regions[entry.first]["Vm"] = entry.second.value;
    }
    for (const auto& entry: cell.get_region_axial_resistivity()) {
        regions[entry.first]["Ra"] = entry.second.value;
    }
    for (const auto& entry: cell.get_region_membrane_capacitance()) {
         regions[entry.first]["cm"] = entry.second.value;
    }
    for (const auto& entry: cell.get_region_initial_ion_data()) {
        nlohmann::json ion_data;
        for (auto ion: entry.second) {
            nlohmann::json data;
            data["internal-concentration"] =ion.initial.init_int_concentration;
            data["external-concentration"] = ion.initial.init_ext_concentration;
            data["reversal-potential"] = ion.initial.init_reversal_potential;
            ion_data[ion.ion] = data;
        }
        regions[entry.first]["ions"] = ion_data;
    }

    std::vector<nlohmann::json> reg_vec;
    for (auto reg: regions) {
        reg.second["region"] = reg.first;
        reg_vec.push_back(reg.second);
    }

    json_file["local"] = reg_vec;

    // Mechs
    std::vector<nlohmann::json> mechs;
    for (const auto& entry: cell.get_region_mechanism_desc()) {
        auto reg = entry.first;
        for (auto& mech_desc: entry.second) {
            nlohmann::json data;
            data["region"] = reg;
            data["mechanism"] = mech_desc.name();
            data["parameters"] = mech_desc.values();
            mechs.push_back(data);
        }
    }

    json_file["mechanisms"] = mechs;

    std::ofstream file(file_name);
    file << std::setw(2) << json_file;
}

void register_param_loader(pybind11::module& m) {
    m.def("load_cell_default_parameters",
          [](std::string fname) {
              std::ifstream fid{fname};
              if (!fid.good()) {
                  throw pyarb_error(util::pprintf("can't open file '{}'", fname));
              }
              nlohmann::json defaults_json;
              defaults_json << fid;
              auto defaults = load_cell_parameters(defaults_json);
              try {
                  check_defaults(defaults);
              }
              catch (std::exception& e) {
                  throw pyarb_error("error loading parameter from \"" + fname + "\": " + std::string(e.what()));
              }
              return defaults;
          },
          "Load default cell parameters.");

    m.def("load_cell_global_parameters",
          [](std::string fname) {
              std::ifstream fid{fname};
              if (!fid.good()) {
                  throw pyarb_error(util::pprintf("can't open file '{}'", fname));
              }
              nlohmann::json cells_json;
              cells_json << fid;
              auto globals_json = find_and_remove_json<nlohmann::json>("global", cells_json);
              if (globals_json) {
                  return load_cell_parameters(globals_json.value());
              }
              return arb::cable_cell_parameter_set();
          },
          "Load global cell parameters.");

    m.def("load_cell_local_parameter_map",
          [](std::string fname) {
              std::unordered_map<std::string, arb::cable_cell_parameter_set> local_map;

              std::ifstream fid{fname};
              if (!fid.good()) {
                  throw pyarb_error(util::pprintf("can't open file '{}'", fname));
              }
              nlohmann::json cells_json;
              cells_json << fid;

              auto locals_json = find_and_remove_json<std::vector<nlohmann::json>>("local", cells_json);
              if (locals_json) {
                  for (auto l: locals_json.value()) {
                      auto region = find_and_remove_json<std::string>("region", l);
                      if (!region) {
                          throw pyarb_error("Local cell parameters do not include region label (in \"" + fname + "\")");
                      }
                      auto region_params = load_cell_parameters(l);

                      if(!region_params.reversal_potential_method.empty()) {
                          throw pyarb_error("Cannot implement local reversal potential methods (in \"" + fname + "\")");
                      }

                      local_map[region.value()] = region_params;
                  }
              }
              return local_map;
          },
          "Load local cell parameters.");

    m.def("load_cell_mechanism_map",
          [](std::string fname) {
              std::unordered_map<std::string, std::vector<arb::mechanism_desc>> mech_map;

              std::ifstream fid{fname};
              if (!fid.good()) {
                  throw pyarb_error(util::pprintf("can't open file '{}'", fname));
              }
              nlohmann::json cells_json;
              cells_json << fid;

              auto mech_json = find_and_remove_json<std::vector<nlohmann::json>>("mechanisms", cells_json);
              if (mech_json) {
                  for (auto m: mech_json.value()) {
                      auto region = find_and_remove_json<std::string>("region", m);
                      if (!region) {
                          throw pyarb_error("Mechanisms do not include region label (in \"" + fname + "\")");
                      }
                      try {
                          mech_map[region.value()].push_back(load_mechanism_desc(m));
                      }
                      catch (std::exception& e) {
                          throw pyarb_error("error loading mechanism for region " + region.value() + " in file \"" + fname + "\": " + std::string(e.what()));
                      }
                  }
              }
              return mech_map;
          },
          "Load region mechanism descriptions.");

    m.def("write_cell_params",
          [](const arb::cable_cell& cell, std::string file_name) {
              output_cell_params(cell, file_name);
          },
          "write our region parameters.");

    // arb::cable_cell_parameter_set
    pybind11::class_<arb::cable_cell_parameter_set> cable_cell_parameter_set(m, "cable_cell_parameter_set");
    cable_cell_parameter_set
            .def("__repr__", [](const arb::cable_cell_parameter_set& s) { return util::pprintf("<arbor.cable_cell_parameter_set>"); })
            .def("__str__", [](const arb::cable_cell_parameter_set& s) { return util::pprintf("(cell_parameter_set)"); });


    // map of arb::cable_cell_parameter_set
    pybind11::class_<std::unordered_map<std::string, arb::cable_cell_parameter_set>> region_parameter_map(m, "region_parameter_map");

    // map of arb::cable_cell_parameter_set
    pybind11::class_<std::unordered_map<std::string, std::vector<arb::mechanism_desc>>> region_mechanism_map(m, "region_mechanism_map");
}

} //namespace pyarb