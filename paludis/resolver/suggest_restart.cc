/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2009 Ciaran McCreesh
 *
 * This file is part of the Paludis package manager. Paludis is free software;
 * you can redistribute it and/or modify it under the terms of the GNU General
 * Public License version 2, as published by the Free Software Foundation.
 *
 * Paludis is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <paludis/resolver/suggest_restart.hh>
#include <paludis/resolver/resolvent.hh>
#include <paludis/resolver/constraint.hh>
#include <paludis/util/private_implementation_pattern-impl.hh>
#include <paludis/util/stringify.hh>

using namespace paludis;
using namespace paludis::resolver;

namespace paludis
{
    template <>
    struct Implementation<SuggestRestart>
    {
        const Resolvent resolvent;
        const std::tr1::shared_ptr<const Decision> previous_decision;
        const std::tr1::shared_ptr<const Constraint> problematic_constraint;
        const std::tr1::shared_ptr<const Decision> new_decision;
        const std::tr1::shared_ptr<const Constraint> suggested_preset;

        Implementation(const Resolvent & q,
                const std::tr1::shared_ptr<const Decision> & pd,
                const std::tr1::shared_ptr<const Constraint> & pc,
                const std::tr1::shared_ptr<const Decision> & nd,
                const std::tr1::shared_ptr<const Constraint> & nc
                ) :
            resolvent(q),
            previous_decision(pd),
            problematic_constraint(pc),
            new_decision(nd),
            suggested_preset(nc)
        {
        }
    };
}

SuggestRestart::SuggestRestart(const Resolvent & q,
        const std::tr1::shared_ptr<const Decision> & pd,
        const std::tr1::shared_ptr<const Constraint> & pc,
        const std::tr1::shared_ptr<const Decision> & nd,
        const std::tr1::shared_ptr<const Constraint> & nc
        ) throw () :
    PrivateImplementationPattern<SuggestRestart>(new Implementation<SuggestRestart>(q, pd, pc, nd, nc)),
    Exception("Suggesting restart with " + stringify(nc->spec()) + " for " + stringify(q))
{
}

SuggestRestart::SuggestRestart(const SuggestRestart & o) :
    PrivateImplementationPattern<SuggestRestart>(new Implementation<SuggestRestart>(
                o.resolvent(), o.previous_decision(), o.problematic_constraint(),
                o.new_decision(), o.suggested_preset())),
    Exception(o)
{
}

SuggestRestart::~SuggestRestart() throw ()
{
}

const Resolvent
SuggestRestart::resolvent() const
{
    return _imp->resolvent;
}

const std::tr1::shared_ptr<const Decision>
SuggestRestart::previous_decision() const
{
    return _imp->previous_decision;
}

const std::tr1::shared_ptr<const Constraint>
SuggestRestart::problematic_constraint() const
{
    return _imp->problematic_constraint;
}

const std::tr1::shared_ptr<const Decision>
SuggestRestart::new_decision() const
{
    return _imp->new_decision;
}

const std::tr1::shared_ptr<const Constraint>
SuggestRestart::suggested_preset() const
{
    return _imp->suggested_preset;
}

template class PrivateImplementationPattern<SuggestRestart>;

