#pragma once

#include <cmath>
#include <limits>

#include <mechanism.hpp>
#include <algorithms.hpp>
#include <util/pprintf.hpp>

namespace nest{ namespace mc{ namespace mechanisms{ namespace expsyn_proto{

template<class Backend>
class mechanism_expsyn : public mechanism<Backend> {
public:
    using base = mechanism<Backend>;
    using value_type  = typename base::value_type;
    using size_type   = typename base::size_type;

    using array = typename base::array;
    using iarray  = typename base::iarray;
    using view   = typename base::view;
    using iview  = typename base::iview;
    using const_iview = typename base::const_iview;
    using indexed_view_type= typename base::indexed_view_type;
    using ion_type = typename base::ion_type;


    mechanism_expsyn(view vec_v, view vec_i, array&& weights, iarray&& node_index)
    :   base(vec_v, vec_i, std::move(node_index))
    {
        size_type num_fields = 3;

        // calculate the padding required to maintain proper alignment of sub arrays
        auto alignment  = data_.alignment();
        auto field_size_in_bytes = sizeof(value_type)*size();
        auto remainder  = field_size_in_bytes % alignment;
        auto padding    = remainder ? (alignment - remainder)/sizeof(value_type) : 0;
        auto field_size = size()+padding;

        // allocate memory
        data_ = array(field_size*num_fields, std::numeric_limits<value_type>::quiet_NaN());

        // asign the sub-arrays
        tau             = data_(0*field_size, 1*size());
        e               = data_(1*field_size, 2*size());
        g               = data_(2*field_size, 3*size());

        // set initial values for variables and parameters
        std::fill(tau.data(), tau.data()+size(), 2);
        std::fill(e.data(), e.data()+size(), 0);

    }

    using base::size;

    std::size_t memory() const override {
        auto s = std::size_t{0};
        s += data_.size()*sizeof(value_type);
        return s;
    }

    void set_params(value_type t_, value_type dt_) override {
        t = t_;
        dt = dt_;
    }

    std::string name() const override {
        return "expsyn";
    }

    mechanismKind kind() const override {
        return mechanismKind::point;
    }

    bool uses_ion(ionKind k) const override {
        switch(k) {
            case ionKind::na : return false;
            case ionKind::ca : return false;
            case ionKind::k  : return false;
        }
        return false;
    }

    void set_ion(ionKind k, ion_type& i, std::vector<size_type>const& index) override {
        using nest::mc::algorithms::index_into;
        throw std::domain_error(nest::mc::util::pprintf("mechanism % does not support ion type\n", name()));
    }

    void nrn_current() override {
        const indexed_view_type vec_v(vec_v_, node_index_);
        indexed_view_type vec_i(vec_i_, node_index_);
        int n_ = node_index_.size();
        for(int i_=0; i_<n_; ++i_) {
            value_type v = vec_v[i_];
            value_type current_, i;
            i = g[i_]*(v-e[i_]);
            current_ = i;
            vec_i[i_] += current_;
        }
    }

    void nrn_init() override {
        int n_ = node_index_.size();
        for(int i_=0; i_<n_; ++i_) {
            g[i_] =  0;
        }
    }

    void net_receive(int i_, value_type weight) override {
        g[i_] = g[i_]+weight;
    }

    void nrn_state() override {
        int n_ = node_index_.size();
        for(int i_=0; i_<n_; ++i_) {
            value_type ba_, a_;
            a_ =  -1/tau[i_];
            ba_ =  0/a_;
            g[i_] =  -ba_+(g[i_]+ba_)*exp(a_*dt);
        }
    }

    array data_;
    view tau;
    view e;
    view g;
    value_type t = 0;
    value_type dt = 0;

    using base::vec_v_;
    using base::vec_i_;
    using base::node_index_;

};

}}}} // namespaces
