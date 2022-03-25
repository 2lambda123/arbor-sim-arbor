#pragma once

#include <type_traits>

#include <arbor/assert.hpp>

#include <memory/memory.hpp>
#include <util/span.hpp>

namespace arb {

/// Hines matrix
/// Make the back end state implementation optional to allow for
/// testing different implementations in the same code.
template<class Backend, class State=typename Backend::matrix_state>
class matrix {
public:
    using backend = Backend;

    // define basic types
    using value_type = typename backend::value_type;
    using index_type = typename backend::index_type;
    using size_type = typename backend::size_type;

    // define storage types
    using array = typename backend::array;
    using iarray = typename backend::iarray;

    // back end specific storage for matrix state
    using state = State;

    matrix() = default;

    matrix(const std::vector<index_type>& pi,
           const std::vector<index_type>& ci,
           const std::vector<value_type>& cv_capacitance,
           const std::vector<value_type>& face_conductance,
           const std::vector<value_type>& cv_area):
        parent_index_(pi.begin(), pi.end()),
        cell_index_(ci.begin(), ci.end()),
        state_(pi, ci, cv_capacitance, face_conductance, cv_area)
    {
        arb_assert(cell_index_[num_cells()] == index_type(parent_index_.size()));
    }

    /// the dimension of the matrix (i.e. the number of rows or colums)
    std::size_t size() const {
        return parent_index_.size();
    }

    /// the number of cell matrices that have been packed together
    size_type num_cells() const {
        return cell_index_.size() - 1;
    }

    /// the vector holding the parent index
    const iarray& p() const { return parent_index_; }

    /// the partition of the parent index over the cells
    const iarray& cell_index() const { return cell_index_; }

    /// Solve the linear system into a given solution storage.
    void solve(array& to) {
        state_.solve(to);
    }

    /// Assemble the matrix for given dt
    void assemble(const value_type& dt, const array& voltage, const array& current, const array& conductivity) {
        state_.assemble(dt, voltage, current, conductivity);
    }

private:
    /// the parent indice that describe matrix structure
    iarray parent_index_;

    /// indexes that point to the start of each cell in the index
    iarray cell_index_;

public:
    // Provide via public interface to make testing much easier.
    // If you modify this directly without knowing what you are doing,
    // you get what you deserve.
    state state_;
};

} // namespace arb
