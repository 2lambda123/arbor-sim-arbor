#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <arbor/benchmark_cell.hpp>
#include <arbor/cable_cell.hpp>
#include <arbor/lif_cell.hpp>
#include <arbor/morph/label_dict.hpp>
#include <arbor/schedule.hpp>
#include <arbor/spike_source_cell.hpp>
#include <arbor/util/any.hpp>
#include <arbor/util/unique_any.hpp>

#include "conversion.hpp"
#include "error.hpp"
#include "morph_parse.hpp"
#include "schedule.hpp"
#include "strprintf.hpp"

/*
 *
 *  - Cell builder for ring test
 *
 */

namespace pyarb {

// Convert a cell description inside a Python object to a cell
// description in a unique_any, as required by the recipe interface.
//
// Warning: requires that the GIL has been acquired before calling,
// if there is a segmentation fault in the cast or isinstance calls,
// check that the caller has the GIL.
arb::util::unique_any convert_cell(pybind11::object o) {
    using pybind11::isinstance;
    using pybind11::cast;

    pybind11::gil_scoped_acquire guard;
    if (isinstance<arb::spike_source_cell>(o)) {
        return arb::util::unique_any(cast<arb::spike_source_cell>(o));
    }
    if (isinstance<arb::benchmark_cell>(o)) {
        return arb::util::unique_any(cast<arb::benchmark_cell>(o));
    }
    if (isinstance<arb::lif_cell>(o)) {
        return arb::util::unique_any(cast<arb::lif_cell>(o));
    }
    if (isinstance<arb::cable_cell>(o)) {
        return arb::util::unique_any(cast<arb::cable_cell>(o));
    }

    throw pyarb_error("recipe.cell_description returned \""
                      + std::string(pybind11::str(o))
                      + "\" which does not describe a known Arbor cell type");
}

//
//  proxies
//

struct label_dict_proxy {
    using str_map = std::unordered_map<std::string, std::string>;
    arb::label_dict dict;
    str_map cache;
    std::vector<std::string> locsets;
    std::vector<std::string> regions;

    label_dict_proxy() = default;

    label_dict_proxy(const str_map& in) {
        for (auto& i: in) {
            set(i.first.c_str(), i.second.c_str());
        }
    }

    std::size_t size() const {
        return dict.size();
    }

    void set(const char* name, const char* desc) {
        using namespace std::string_literals;
        try{
            if (!test_identifier(name)) {
                throw std::string(util::pprintf("'{}' is not a valid label name.", name));
            }
            auto result = eval(parse(desc));
            if (!result) {
                // there was a well-defined
                throw std::string(result.error().message);
            }
            else if (result->type()==typeid(arb::region)) {
                dict.set(name, std::move(arb::util::any_cast<arb::region&>(*result)));
                auto it = std::lower_bound(regions.begin(), regions.end(), name);
                if (it==regions.end() || *it!=name) regions.insert(it, name);
            }
            else if (result->type()==typeid(arb::locset)) {
                dict.set(name, std::move(arb::util::any_cast<arb::locset&>(*result)));
                auto it = std::lower_bound(locsets.begin(), locsets.end(), name);
                if (it==locsets.end() || *it!=name) locsets.insert(it, name);
            }
            else {
                // I don't know what I just parsed!
                throw util::pprintf("The defninition of '{} = {}' does not define a valid region or locset.", name, desc);
            }
            // The entry was added succesfully: store it in the cache.
            cache[name] = desc;
        }
        catch (std::string msg) {
            const char* base = "\nError adding the label '{}' = '{}'\n{}\n";

            throw std::runtime_error(util::pprintf(base, name, desc, msg));
        }
        // parse_errors: line/column information
        // std::exception: all others
        catch (std::exception& e) {
            const char* msg =
                "\n----- internal error -------------------------------------------"
                "\nError parsing the label: '{}' = '{}'"
                "\n"
                "\n{}"
                "\n"
                "\nPlease file a bug report with this full error message at:"
                "\n    github.com/arbor-sim/arbor/issues"
                "\n----------------------------------------------------------------";
            throw arb::arbor_internal_error(util::pprintf(msg, name, desc, e.what()));
        }
    }
};

//
// string printers
//

std::string lif_str(const arb::lif_cell& c){
    return util::pprintf(
        "<arbor.lif_cell: tau_m {}, V_th {}, C_m {}, E_L {}, V_m {}, t_ref {}, V_reset {}>",
        c.tau_m, c.V_th, c.C_m, c.E_L, c.V_m, c.t_ref, c.V_reset);
}


std::string mechanism_desc_str(const arb::mechanism_desc& md) {
    return util::pprintf("<arbor.mechanism: name '{}', parameters {}",
            md.name(), util::dictionary_csv(md.values()));
}

std::ostream& operator<<(std::ostream& o, const arb::cable_cell_ion_data& d) {
    return o << util::pprintf("(con_in {}, con_ex {}, rev_pot {})",
            d.init_int_concentration, d.init_ext_concentration, d.init_reversal_potential);
}

std::string ion_data_str(const arb::cable_cell_ion_data& d) {
    return util::pprintf(
        "<arbor.cable_cell_ion_data: con_in {}, con_ex {}, rev_pot {}>",
        d.init_int_concentration, d.init_ext_concentration, d.init_reversal_potential);
}

void register_cells(pybind11::module& m) {
    using namespace pybind11::literals;

    // arb::spike_source_cell

    pybind11::class_<arb::spike_source_cell> spike_source_cell(m, "spike_source_cell",
        "A spike source cell, that generates a user-defined sequence of spikes that act as inputs for other cells in the network.");

    spike_source_cell
        .def(pybind11::init<>(
            [](const regular_schedule_shim& sched){
                return arb::spike_source_cell{sched.schedule()};}),
            "schedule"_a, "Construct a spike source cell that generates spikes at regular intervals.")
        .def(pybind11::init<>(
            [](const explicit_schedule_shim& sched){
                return arb::spike_source_cell{sched.schedule()};}),
            "schedule"_a, "Construct a spike source cell that generates spikes at a sequence of user-defined times.")
        .def(pybind11::init<>(
            [](const poisson_schedule_shim& sched){
                return arb::spike_source_cell{sched.schedule()};}),
            "schedule"_a, "Construct a spike source cell that generates spikes at times defined by a Poisson sequence.")
        .def("__repr__", [](const arb::spike_source_cell&){return "<arbor.spike_source_cell>";})
        .def("__str__",  [](const arb::spike_source_cell&){return "<arbor.spike_source_cell>";});

    // arb::benchmark_cell

    pybind11::class_<arb::benchmark_cell> benchmark_cell(m, "benchmark_cell",
        "A benchmarking cell, used by Arbor developers to test communication performance.\n"
        "A benchmark cell generates spikes at a user-defined sequence of time points, and\n"
        "the time taken to integrate a cell can be tuned by setting the realtime_ratio,\n"
        "for example if realtime_ratio=2, a cell will take 2 seconds of CPU time to\n"
        "simulate 1 second.\n");

    benchmark_cell
        .def(pybind11::init<>(
            [](const regular_schedule_shim& sched, double ratio){
                return arb::benchmark_cell{sched.schedule(), ratio};}),
            "schedule"_a, "realtime_ratio"_a=1.0,
            "Construct a benchmark cell that generates spikes at regular intervals.")
        .def(pybind11::init<>(
            [](const explicit_schedule_shim& sched, double ratio){
                return arb::benchmark_cell{sched.schedule(), ratio};}),
            "schedule"_a, "realtime_ratio"_a=1.0,
            "Construct a benchmark cell that generates spikes at a sequence of user-defined times.")
        .def(pybind11::init<>(
            [](const poisson_schedule_shim& sched, double ratio){
                return arb::benchmark_cell{sched.schedule(), ratio};}),
            "schedule"_a, "realtime_ratio"_a=1.0,
            "Construct a benchmark cell that generates spikes at times defined by a Poisson sequence.")
        .def("__repr__", [](const arb::benchmark_cell&){return "<arbor.benchmark_cell>";})
        .def("__str__",  [](const arb::benchmark_cell&){return "<arbor.benchmark_cell>";});

    // arb::lif_cell

    pybind11::class_<arb::lif_cell> lif_cell(m, "lif_cell",
        "A benchmarking cell, used by Arbor developers to test communication performance.");

    lif_cell
        .def(pybind11::init<>())
        .def_readwrite("tau_m", &arb::lif_cell::tau_m,
                "Membrane potential decaying constant [ms].")
        .def_readwrite("V_th", &arb::lif_cell::V_th,
                       "Firing threshold [mV].")
        .def_readwrite("C_m", &arb::lif_cell::C_m,
                      " Membrane capacitance [pF].")
        .def_readwrite("E_L", &arb::lif_cell::E_L,
                       "Resting potential [mV].")
        .def_readwrite("V_m", &arb::lif_cell::V_m,
                       "Initial value of the Membrane potential [mV].")
        .def_readwrite("t_ref", &arb::lif_cell::t_ref,
                       "Refractory period [ms].")
        .def_readwrite("V_reset", &arb::lif_cell::V_reset,
                       "Reset potential [mV].")
        .def("__repr__", &lif_str)
        .def("__str__",  &lif_str);

    //
    // regions and locsets
    //

    // arb::label_dict

    pybind11::class_<label_dict_proxy> label_dict(m, "label_dict");
    label_dict
        .def(pybind11::init<>())
        .def(pybind11::init<const std::unordered_map<std::string, std::string>&>())
        .def("__setitem__",
            [](label_dict_proxy& l, const char* name, const char* desc) {
                l.set(name, desc);})
        .def("__getitem__",
            [](label_dict_proxy& l, const char* name) {
                if (!l.cache.count(name)) {
                    throw std::runtime_error(util::pprintf("\nKeyError: '{}'", name));
                }
                return l.cache.at(name);
            })
        .def("__len__", &label_dict_proxy::size)
        .def("__iter__",
            [](const label_dict_proxy &ld) {
                return pybind11::make_key_iterator(ld.cache.begin(), ld.cache.end());},
            pybind11::keep_alive<0, 1>())
        .def_readonly("regions", &label_dict_proxy::regions,
             "The region labels stored in a set.")
        .def_readonly("locsets", &label_dict_proxy::locsets,
             "The locset labels stored in a set.")
        .def("__str__",  [](const label_dict_proxy&){return std::string("dictionary");})
        .def("__repr__", [](const label_dict_proxy&){return std::string("dictionary");});

    //
    // Data structures used to describe mechanisms, electrical properties,
    // gap junction properties, etc.
    //

    // arb::mechanism_desc
    pybind11::class_<arb::mechanism_desc> mechanism_desc(m, "mechanism");
    mechanism_desc
        .def(pybind11::init([](const char* n) {return arb::mechanism_desc{n};}))
        // allow construction of a description with parameters provided in a dictionary:
        //      mech = arbor.mechanism('mech_name', {'param1': 1.2, 'param2': 3.14})
        .def(pybind11::init(
            [](const char* name, std::unordered_map<std::string, double> params) {
                arb::mechanism_desc md(name);
                for (const auto& p: params) md.set(p.first, p.second);
                return md;
            }))
        .def("set",
            [](arb::mechanism_desc& md, std::string key, double value) {
                md.set(key, value);
            },
            "key"_a, "value"_a)
        .def_property_readonly("name", [](const arb::mechanism_desc& md) {return md.name();})
        .def_property_readonly("values", [](const arb::mechanism_desc& md) {return md.values();})
        .def("__repr__", &mechanism_desc_str)
        .def("__str__",  &mechanism_desc_str);

    // arb::gap_junction_site
    pybind11::class_<arb::gap_junction_site> gjsite(m, "gap_junction");
    gjsite
        .def(pybind11::init<>())
        .def("__repr__", [](const arb::gap_junction_site&){return "<arbor.gap_junction>";})
        .def("__str__", [](const arb::gap_junction_site&){return "<arbor.gap_junction>";});

    // arb::i_clamp
    pybind11::class_<arb::i_clamp> i_clamp(m, "iclamp");
    i_clamp
        .def(pybind11::init(
                [](double del, double dur, double amp) {
                    return arb::i_clamp{del, dur, amp};
                }), "delay"_a=0, "duration"_a=0, "amplitude"_a=0)
        .def_readonly("delay", &arb::i_clamp::delay,         "Delay before current starts [ms]")
        .def_readonly("duration", &arb::i_clamp::duration,   "Duration of the current injection [ms]")
        .def_readonly("amplitude", &arb::i_clamp::amplitude, "Amplitude of the injected current [nA]")
        .def("__repr__", [](const arb::i_clamp& c){
            return util::pprintf("<arbor.iclamp: delay {} ms, duration {} ms, amplitude {} nA>", c.delay, c.duration, c.amplitude);})
        .def("__str__", [](const arb::i_clamp& c){
            return util::pprintf("<arbor.iclamp: delay {} ms, duration {} ms, amplitude {} nA>", c.delay, c.duration, c.amplitude);});

    // arb::threshold_detector
    pybind11::class_<arb::threshold_detector> detector(m, "spike_detector",
            "A spike detector that generates a spike when voltage crosses a threshold.");
    detector
        .def(pybind11::init(
                [](double thresh) {
                    return arb::threshold_detector{thresh};
                }), "threshold"_a)
        .def_readonly("threshold", &arb::threshold_detector::threshold, "Voltage threshold of spike detector [ms]")
        .def("__repr__", [](const arb::threshold_detector& d){
            return util::pprintf("<arbor.threshold_detector: threshold {} mV>", d.threshold);})
        .def("__str__", [](const arb::threshold_detector& d){
            return util::pprintf("<arbor.threshold_detector: threshold {} mV>", d.threshold);});

    // arb::cable_cell
    pybind11::class_<arb::cable_cell> cable_cell(m, "cable_cell");
    cable_cell
        .def(pybind11::init(
            [](const arb::morphology& m, const label_dict_proxy& labels, bool cfd) {
                return arb::cable_cell(m, labels.dict, cfd);
            }), "morphology"_a, "labels"_a, "compartments_from_discretization"_a=true)
        .def_property_readonly("num_branches", [](const arb::cable_cell& m) {return m.num_branches();})
        // Paint mechanisms.
        .def("paint",
            [](arb::cable_cell& c, const char* region, const arb::mechanism_desc& d) {
                c.paint(region, d);
            },
            "region"_a, "mechanism"_a,
            "Associate a mechanism with a region.")
        .def("paint",
            [](arb::cable_cell& c, const char* region, const char* mech_name) {
                c.paint(region, mech_name);
            },
            "region"_a, "mechanism_name"_a,
            "Associate a mechanism with a region.")
        // TODO: the place calls are not throwing an error when placed on a non-existant cell
        // Place synapses.
        .def("place",
            [](arb::cable_cell& c, const char* locset, const arb::mechanism_desc& d) {
                c.place(locset, d);
            },
            "locations"_a, "mechanism"_a,
            "Associate a synapse with a set of locations.")
        .def("place",
            [](arb::cable_cell& c, const char* locset, const char* mech_name) {
                c.place(locset, mech_name);
            },
            "locations"_a, "mechanism_name"_a,
            "Associate a synapse with a set of locations.")
        // Place gap junctions.
        .def("place",
            [](arb::cable_cell& c, const char* locset, const arb::gap_junction_site& site) {
                c.place(locset, site);
            },
            "locations"_a, "gapjunction"_a,
            "Add a set of gap junction locations.")
        // Place current clamp stimulus.
        .def("place",
            [](arb::cable_cell& c, const char* locset, const arb::i_clamp& stim) {
                c.place(locset, stim);
            },
            "locations"_a, "iclamp"_a,
            "Add a stimulus to each location in locations.")
        // Place spike detector.
        .def("place",
            [](arb::cable_cell& c, const char* locset, const arb::threshold_detector& d) {
                c.place(locset, d);
            },
            "locations"_a, "detector"_a,
            "Add a voltage threshold detector (spike detector) to each location in locations.")
        .def("__repr__", [](const arb::cable_cell&){return "<arbor.cable_cell>";})
        .def("__str__",  [](const arb::cable_cell&){return "<arbor.cable_cell>";});
}

} // namespace pyarb
