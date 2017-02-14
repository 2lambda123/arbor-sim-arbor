#pragma once

// Transform derivative block into AST representing
// an integration step over the state variables, based on
// solver method.

#include <string>
#include <vector>

#include "expression.hpp"
#include "symdiff.hpp"
#include "symge.hpp"
#include "visitor.hpp"

expression_ptr remove_unused_locals(BlockExpression* block);

class DirectSolverVisitor : public BlockRewriterBase {
public:
    using BlockRewriterBase::visit;

    DirectSolverVisitor() {}
    DirectSolverVisitor(scope_ptr enclosing): BlockRewriterBase(enclosing) {}

    virtual void visit(AssignmentExpression *e) override {
        // No solver method, so declare an error if lhs is a derivative.
        if(auto deriv = e->lhs()->is_derivative()) {
            error({"The DERIVATIVE block has a derivative expression"
                   " but no METHOD was specified in the SOLVE statement",
                   deriv->location()});
        }
    }
};

class CnexpSolverVisitor : public BlockRewriterBase {
protected:
    // list of identifier names appearing in derivatives on lhs
    std::vector<std::string> dvars_;

public:
    using BlockRewriterBase::visit;

    CnexpSolverVisitor() {}
    CnexpSolverVisitor(scope_ptr enclosing): BlockRewriterBase(enclosing) {}

    virtual void visit(BlockExpression* e) override;
    virtual void visit(AssignmentExpression *e) override;

    virtual void reset() override {
        dvars_.clear();
        BlockRewriterBase::reset();
    }
};

class SparseSolverVisitor : public BlockRewriterBase {
protected:
    // List of identifier names appearing in derivatives on lhs.
    std::vector<std::string> dvars_;

    // 'Current' differential equation is for variable with this
    // index in `dvars`.
    unsigned deq_index_ = 0;

    // Expanded local assignments that need to be substituted in for derivative
    // calculations.
    substitute_map local_expr_;

    // Symbolic matrix for backwards Euler step.
    symge::sym_matrix A_;

    // 'Symbol table' for symbolic manipulation.
    symge::symbol_table symtbl_;

public:
    using BlockRewriterBase::visit;

    SparseSolverVisitor() {}

    SparseSolverVisitor(scope_ptr enclosing): BlockRewriterBase(enclosing) {}

    virtual void visit(BlockExpression* e) override;
    virtual void visit(AssignmentExpression *e) override;
    virtual void finalize() override;
    virtual void reset() override;
};
