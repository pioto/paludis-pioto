/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2009, 2010 Ciaran McCreesh
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

#include <paludis/resolver/decider.hh>
#include <paludis/resolver/resolver_functions.hh>
#include <paludis/resolver/spec_rewriter.hh>
#include <paludis/resolver/resolvent.hh>
#include <paludis/resolver/resolution.hh>
#include <paludis/resolver/constraint.hh>
#include <paludis/resolver/decision.hh>
#include <paludis/resolver/destination.hh>
#include <paludis/resolver/resolutions.hh>
#include <paludis/resolver/suggest_restart.hh>
#include <paludis/resolver/reason.hh>
#include <paludis/resolver/unsuitable_candidates.hh>
#include <paludis/resolver/resolver.hh>
#include <paludis/resolver/resolver_lists.hh>
#include <paludis/util/exception.hh>
#include <paludis/util/stringify.hh>
#include <paludis/util/make_named_values.hh>
#include <paludis/util/make_shared_ptr.hh>
#include <paludis/util/wrapped_forward_iterator.hh>
#include <paludis/util/wrapped_output_iterator.hh>
#include <paludis/util/enum_iterator.hh>
#include <paludis/util/indirect_iterator-impl.hh>
#include <paludis/environment.hh>
#include <paludis/notifier_callback.hh>
#include <paludis/repository.hh>
#include <paludis/filtered_generator.hh>
#include <paludis/package_database.hh>
#include <paludis/metadata_key.hh>
#include <paludis/generator.hh>
#include <paludis/selection.hh>
#include <paludis/filter.hh>
#include <paludis/match_package.hh>
#include <paludis/version_requirements.hh>
#include <paludis/slot_requirement.hh>
#include <paludis/choice.hh>
#include <paludis/action.hh>
#include <paludis/elike_slot_requirement.hh>

#include <paludis/util/private_implementation_pattern-impl.hh>

#include <algorithm>
#include <map>
#include <set>

using namespace paludis;
using namespace paludis::resolver;

typedef std::map<Resolvent, std::tr1::shared_ptr<Resolution> > ResolutionsByResolventMap;

namespace paludis
{
    template <>
    struct Implementation<Decider>
    {
        const Environment * const env;
        const ResolverFunctions fns;
        SpecRewriter rewriter;

        ResolutionsByResolventMap resolutions_by_resolvent;

        const std::tr1::shared_ptr<ResolverLists> lists;

        Implementation(const Environment * const e, const ResolverFunctions & f,
                const std::tr1::shared_ptr<ResolverLists> & l) :
            env(e),
            fns(f),
            rewriter(env),
            lists(l)
        {
        }
    };
}

Decider::Decider(const Environment * const e, const ResolverFunctions & f,
        const std::tr1::shared_ptr<ResolverLists> & l) :
    PrivateImplementationPattern<Decider>(new Implementation<Decider>(e, f, l))
{
}

Decider::~Decider()
{
}

void
Decider::_resolve_decide_with_dependencies()
{
    Context context("When resolving and adding dependencies recursively:");

    enum State { deciding_non_suggestions, deciding_suggestions, finished } state = deciding_non_suggestions;
    bool changed(true);
    while (true)
    {
        if (! changed)
            state = State(state + 1);
        if (state == finished)
            break;

        changed = false;
        for (ResolutionsByResolventMap::iterator i(_imp->resolutions_by_resolvent.begin()),
                i_end(_imp->resolutions_by_resolvent.end()) ;
                i != i_end ; ++i)
        {
            /* we've already decided */
            if (i->second->decision())
                continue;

            /* we're only being suggested. don't do this on the first pass, so
             * we don't have to do restarts for suggestions later becoming hard
             * deps. */
            if (state == deciding_non_suggestions && i->second->constraints()->all_untaken())
                continue;

            _imp->env->trigger_notifier_callback(NotifierCallbackResolverStepEvent());

            changed = true;
            _decide(i->first, i->second);

            _add_dependencies_if_necessary(i->first, i->second);
        }
    }
}

bool
Decider::_resolve_dependents()
{
    Context context("When finding dependents:");

    bool changed(false);
    const std::pair<
        std::tr1::shared_ptr<const PackageIDSequence>,
        std::tr1::shared_ptr<const PackageIDSequence> > changing(_collect_changing());

    if (changing.first->empty())
        return false;

    const std::tr1::shared_ptr<const PackageIDSequence> staying(_collect_staying(changing.first));

    for (PackageIDSequence::ConstIterator s(staying->begin()), s_end(staying->end()) ;
            s != s_end ; ++s)
    {
        _imp->env->trigger_notifier_callback(NotifierCallbackResolverStepEvent());

        if (_allowed_to_break(*s))
            continue;

        const std::tr1::shared_ptr<const PackageIDSequence> dependent_upon(_dependent_upon(
                    *s, changing.first, changing.second));
        if (dependent_upon->empty())
            continue;

        if (_remove_if_dependent(*s))
        {
            Resolvent resolvent(*s, dt_install_to_slash);

            /* we've changed things if we've not already done anything for this resolvent */
            if (_imp->resolutions_by_resolvent.end() == _imp->resolutions_by_resolvent.find(resolvent))
                changed = true;

            const std::tr1::shared_ptr<Resolution> resolution(_resolution_for_resolvent(resolvent, true));
            const std::tr1::shared_ptr<const ConstraintSequence> constraints(_make_constraints_for_dependent(
                        resolvent, resolution, *s, dependent_upon));
            for (ConstraintSequence::ConstIterator c(constraints->begin()), c_end(constraints->end()) ;
                    c != c_end ; ++c)
                _apply_resolution_constraint(resolvent, resolution, *c);
        }
        else
        {
            throw InternalError(PALUDIS_HERE, "unsafe " + stringify(**s));
        }
    }

    return changed;
}

const std::tr1::shared_ptr<ConstraintSequence>
Decider::_make_constraints_for_dependent(
        const Resolvent & resolvent,
        const std::tr1::shared_ptr<const Resolution> & resolution,
        const std::tr1::shared_ptr<const PackageID> & id,
        const std::tr1::shared_ptr<const PackageIDSequence> & r) const
{
    return _imp->fns.get_constraints_for_dependent_fn()(resolvent, resolution, id, r);
}

namespace
{
    struct DependentChecker
    {
        const Environment * const env;
        const std::tr1::shared_ptr<const PackageIDSequence> going_away;
        const std::tr1::shared_ptr<const PackageIDSequence> newly_available;
        const std::tr1::shared_ptr<PackageIDSequence> result;

        DependentChecker(
                const Environment * const e,
                const std::tr1::shared_ptr<const PackageIDSequence> & g,
                const std::tr1::shared_ptr<const PackageIDSequence> & n) :
            env(e),
            going_away(g),
            newly_available(n),
            result(new PackageIDSequence)
        {
        }

        void visit(const DependencySpecTree::NodeType<NamedSetDepSpec>::Type & s)
        {
            const std::tr1::shared_ptr<const SetSpecTree> set(env->set(s.spec()->name()));
            set->root()->accept(*this);
        }

        void visit(const DependencySpecTree::NodeType<PackageDepSpec>::Type & s)
        {
            for (PackageIDSequence::ConstIterator g(going_away->begin()), g_end(going_away->end()) ;
                    g != g_end ; ++g)
            {
                if (! match_package(*env, *s.spec(), **g, MatchPackageOptions()))
                    continue;

                if (indirect_iterator(newly_available->end()) == std::find_if(
                            indirect_iterator(newly_available->begin()), indirect_iterator(newly_available->end()),
                            std::tr1::bind(&match_package, std::tr1::cref(*env), std::tr1::cref(*s.spec()),
                                std::tr1::placeholders::_1, MatchPackageOptions())))
                    result->push_back(*g);
            }
        }

        void visit(const DependencySpecTree::NodeType<BlockDepSpec>::Type &)
        {
        }

        void visit(const DependencySpecTree::NodeType<ConditionalDepSpec>::Type & s)
        {
            if (s.spec()->condition_met())
                std::for_each(indirect_iterator(s.begin()), indirect_iterator(s.end()),
                        accept_visitor(*this));
        }

        void visit(const DependencySpecTree::NodeType<AnyDepSpec>::Type & s)
        {
            std::for_each(indirect_iterator(s.begin()), indirect_iterator(s.end()),
                    accept_visitor(*this));
        }

        void visit(const DependencySpecTree::NodeType<AllDepSpec>::Type & s)
        {
            std::for_each(indirect_iterator(s.begin()), indirect_iterator(s.end()),
                    accept_visitor(*this));
        }

        void visit(const DependencySpecTree::NodeType<DependenciesLabelsDepSpec>::Type &)
        {
        }
    };
}

const std::tr1::shared_ptr<const PackageIDSequence>
Decider::_dependent_upon(
        const std::tr1::shared_ptr<const PackageID> & id,
        const std::tr1::shared_ptr<const PackageIDSequence> & going_away,
        const std::tr1::shared_ptr<const PackageIDSequence> & staying) const
{
    DependentChecker c(_imp->env, going_away, staying);
    if (id->dependencies_key())
        id->dependencies_key()->value()->root()->accept(c);
    else
    {
        if (id->build_dependencies_key())
            id->build_dependencies_key()->value()->root()->accept(c);
        if (id->run_dependencies_key())
            id->run_dependencies_key()->value()->root()->accept(c);
        if (id->post_dependencies_key())
            id->post_dependencies_key()->value()->root()->accept(c);
        if (id->suggested_dependencies_key())
            id->suggested_dependencies_key()->value()->root()->accept(c);
    }

    return c.result;
}

namespace
{
    struct ChangingCollector
    {
        std::tr1::shared_ptr<PackageIDSequence> going_away;
        std::tr1::shared_ptr<PackageIDSequence> newly_available;

        ChangingCollector() :
            going_away(new PackageIDSequence),
            newly_available(new PackageIDSequence)
        {
        }

        void visit(const NothingNoChangeDecision &)
        {
        }

        void visit(const ExistingNoChangeDecision &)
        {
        }

        void visit(const RemoveDecision & d)
        {
            std::copy(d.ids()->begin(), d.ids()->end(), going_away->back_inserter());
        }

        void visit(const ChangesToMakeDecision &)
        {
            /* todo */
        }

        void visit(const UnableToMakeDecision &)
        {
        }
    };
}

const std::pair<
    std::tr1::shared_ptr<const PackageIDSequence>,
    std::tr1::shared_ptr<const PackageIDSequence> >
Decider::_collect_changing() const
{
    ChangingCollector c;

    for (ResolutionsByResolventMap::const_iterator i(_imp->resolutions_by_resolvent.begin()),
            i_end(_imp->resolutions_by_resolvent.end()) ;
            i != i_end ; ++i)
        if (i->second->decision())
            i->second->decision()->accept(c);

    return std::make_pair(c.going_away, c.newly_available);
}

const std::tr1::shared_ptr<const PackageIDSequence>
Decider::_collect_staying(const std::tr1::shared_ptr<const PackageIDSequence> & going_away) const
{
    const std::tr1::shared_ptr<const PackageIDSequence> existing((*_imp->env)[selection::AllVersionsUnsorted(
                generator::All() | filter::InstalledAtRoot(FSEntry("/")))]);

    const std::tr1::shared_ptr<PackageIDSequence> result(new PackageIDSequence);
    for (PackageIDSequence::ConstIterator x(existing->begin()), x_end(existing->end()) ;
            x != x_end ; ++x)
        if (indirect_iterator(going_away->end()) == std::find(indirect_iterator(going_away->begin()),
                    indirect_iterator(going_away->end()), **x))
            result->push_back(*x);

    return result;
}

void
Decider::_resolve_destinations()
{
    Context context("When resolving destinations:");

    for (ResolutionsByResolventMap::iterator i(_imp->resolutions_by_resolvent.begin()),
            i_end(_imp->resolutions_by_resolvent.end()) ;
            i != i_end ; ++i)
        _do_destination_if_necessary(i->first, i->second);
}

namespace
{
    struct DoDestinationIfNecessaryVisitor
    {
        typedef std::tr1::function<const std::tr1::shared_ptr<const Destination> (
                const ChangesToMakeDecision &)> MakeDestinationFunc;
        typedef std::tr1::function<ChangeType (
                const ChangesToMakeDecision &)> MakeChangeTypeForFunc;

        MakeDestinationFunc make_destination_for;
        MakeChangeTypeForFunc make_change_type_for;

        DoDestinationIfNecessaryVisitor(const MakeDestinationFunc & f, const MakeChangeTypeForFunc & c) :
            make_destination_for(f),
            make_change_type_for(c)
        {
        }

        void visit(RemoveDecision &)
        {
        }

        void visit(ExistingNoChangeDecision &)
        {
        }

        void visit(NothingNoChangeDecision &)
        {
        }

        void visit(UnableToMakeDecision &)
        {
        }

        void visit(ChangesToMakeDecision & decision)
        {
            if (! decision.destination())
            {
                decision.set_destination(make_destination_for(decision));
                decision.set_change_type(make_change_type_for(decision));
            }
        }
    };
}

void
Decider::_do_destination_if_necessary(
        const Resolvent & resolvent,
        const std::tr1::shared_ptr<Resolution> & resolution)
{
    DoDestinationIfNecessaryVisitor v(
            std::tr1::bind(&Decider::_make_destination_for, this, resolvent, resolution, std::tr1::placeholders::_1),
            std::tr1::bind(&Decider::_make_change_type_for, this, resolvent, resolution, std::tr1::placeholders::_1)
            );
    resolution->decision()->accept(v);
}

const std::tr1::shared_ptr<Destination>
Decider::_make_destination_for(
        const Resolvent & resolvent,
        const std::tr1::shared_ptr<const Resolution> & resolution,
        const ChangesToMakeDecision & decision) const
{
    const std::tr1::shared_ptr<const Repository> repo(_find_repository_for(resolvent, resolution, decision));
    if ((! repo->destination_interface()) ||
            (! repo->destination_interface()->is_suitable_destination_for(*decision.origin_id())))
        throw InternalError(PALUDIS_HERE, stringify(repo->name()) + " is not a suitable destination for "
                + stringify(*decision.origin_id()));

    return make_shared_ptr(new Destination(make_named_values<Destination>(
                    value_for<n::replacing>(_find_replacing(decision.origin_id(), repo)),
                    value_for<n::repository>(repo->name())
                    )));
}

const ChangeType
Decider::_make_change_type_for(
        const Resolvent &,
        const std::tr1::shared_ptr<const Resolution> &,
        const ChangesToMakeDecision & decision) const
{
    if (decision.destination()->replacing()->empty())
    {
        const std::tr1::shared_ptr<const PackageIDSequence> others((*_imp->env)[selection::SomeArbitraryVersion(
                    generator::Package(decision.origin_id()->name()) &
                    generator::InRepository(decision.destination()->repository())
                    )]);
        if (others->empty())
            return ct_new;
        else
            return ct_slot_new;
    }
    else
    {
        /* we pick the worst, so replacing 1 and 3 with 2 requires permission to
         * downgrade */
        ChangeType result(last_ct);
        for (PackageIDSequence::ConstIterator i(decision.destination()->replacing()->begin()),
                i_end(decision.destination()->replacing()->end()) ;
                i != i_end ; ++i)
        {
            if ((*i)->version() == decision.origin_id()->version())
                result = std::min(result, ct_reinstall);
            else if ((*i)->version() < decision.origin_id()->version())
                result = std::min(result, ct_upgrade);
            else if ((*i)->version() > decision.origin_id()->version())
                result = std::min(result, ct_downgrade);
        }

        return result;
    }
}

const std::tr1::shared_ptr<const Repository>
Decider::_find_repository_for(const Resolvent & resolvent,
        const std::tr1::shared_ptr<const Resolution> & resolution,
        const ChangesToMakeDecision & decision) const
{
    return _imp->fns.find_repository_for_fn()(resolvent, resolution, decision);
}

FilteredGenerator
Decider::_make_destination_filtered_generator(const Generator & g, const Resolvent & resolvent) const
{
    return _imp->fns.make_destination_filtered_generator_fn()(g, resolvent);
}

const std::tr1::shared_ptr<const PackageIDSequence>
Decider::_find_replacing(
        const std::tr1::shared_ptr<const PackageID> & id,
        const std::tr1::shared_ptr<const Repository> & repo) const
{
    Context context("When working out what is replaced by '" + stringify(*id) +
            "' when it is installed to '" + stringify(repo->name()) + "':");

    std::set<RepositoryName, RepositoryNameComparator> repos;

    if (repo->installed_root_key())
    {
        for (PackageDatabase::RepositoryConstIterator r(_imp->env->package_database()->begin_repositories()),
                r_end(_imp->env->package_database()->end_repositories()) ;
                r != r_end ; ++r)
            if ((*r)->installed_root_key() &&
                    (*r)->installed_root_key()->value() == repo->installed_root_key()->value())
                repos.insert((*r)->name());
    }
    else
        repos.insert(repo->name());

    std::tr1::shared_ptr<PackageIDSequence> result(new PackageIDSequence);
    for (std::set<RepositoryName, RepositoryNameComparator>::const_iterator r(repos.begin()),
            r_end(repos.end()) ;
            r != r_end ; ++r)
    {
        std::tr1::shared_ptr<const PackageIDSequence> ids((*_imp->env)[selection::AllVersionsUnsorted(
                    generator::Package(id->name()) & generator::InRepository(*r))]);
        for (PackageIDSequence::ConstIterator i(ids->begin()), i_end(ids->end()) ;
                i != i_end ; ++i)
        {
            if ((*i)->version() == id->version() || _same_slot(*i, id))
                result->push_back(*i);
        }
    }

    return result;
}

bool
Decider::_same_slot(const std::tr1::shared_ptr<const PackageID> & a,
        const std::tr1::shared_ptr<const PackageID> & b) const
{
    if (a->slot_key())
        return b->slot_key() && a->slot_key()->value() == b->slot_key()->value();
    else
        return ! b->slot_key();
}

const std::tr1::shared_ptr<Resolution>
Decider::_create_resolution_for_resolvent(const Resolvent & r) const
{
    return make_shared_ptr(new Resolution(make_named_values<Resolution>(
                    value_for<n::constraints>(_initial_constraints_for(r)),
                    value_for<n::decision>(make_null_shared_ptr()),
                    value_for<n::resolvent>(r)
                    )));
}

const std::tr1::shared_ptr<Resolution>
Decider::_resolution_for_resolvent(const Resolvent & r, const bool create)
{
    ResolutionsByResolventMap::iterator i(_imp->resolutions_by_resolvent.find(r));
    if (_imp->resolutions_by_resolvent.end() == i)
    {
        if (create)
        {
            std::tr1::shared_ptr<Resolution> resolution(_create_resolution_for_resolvent(r));
            i = _imp->resolutions_by_resolvent.insert(std::make_pair(r, resolution)).first;
            _imp->lists->all_resolutions()->append(resolution);
        }
        else
            throw InternalError(PALUDIS_HERE, "resolver bug: expected resolution for "
                    + stringify(r) + " to exist, but it doesn't");
    }

    return i->second;
}

const std::tr1::shared_ptr<Resolution>
Decider::resolution_for_resolvent(const Resolvent & r) const
{
    ResolutionsByResolventMap::const_iterator i(_imp->resolutions_by_resolvent.find(r));
    if (_imp->resolutions_by_resolvent.end() == i)
        throw InternalError(PALUDIS_HERE, "resolver bug: expected resolution for "
                + stringify(r) + " to exist, but it doesn't");

    return i->second;
}

const std::tr1::shared_ptr<ConstraintSequence>
Decider::_make_constraints_from_target(
        const Resolvent & resolvent,
        const PackageOrBlockDepSpec & spec,
        const std::tr1::shared_ptr<const Reason> & reason) const
{
    if (spec.if_package())
    {
        const std::tr1::shared_ptr<ConstraintSequence> result(new ConstraintSequence);
        result->push_back(make_shared_ptr(new Constraint(make_named_values<Constraint>(
                            value_for<n::destination_type>(resolvent.destination_type()),
                            value_for<n::nothing_is_fine_too>(false),
                            value_for<n::reason>(reason),
                            value_for<n::spec>(spec),
                            value_for<n::untaken>(false),
                            value_for<n::use_existing>(_imp->fns.get_use_existing_fn()(resolvent, *spec.if_package(), reason))
                            ))));
        return result;
    }
    else if (spec.if_block())
        return _make_constraints_from_blocker(resolvent, *spec.if_block(), reason);
    else
        throw InternalError(PALUDIS_HERE, "resolver bug: huh? it's not a block and it's not a package");
}

const std::tr1::shared_ptr<ConstraintSequence>
Decider::_make_constraints_from_dependency(const Resolvent & resolvent, const SanitisedDependency & dep,
        const std::tr1::shared_ptr<const Reason> & reason,
        const SpecInterest interest) const
{
    if (dep.spec().if_package())
    {
        const std::tr1::shared_ptr<ConstraintSequence> result(new ConstraintSequence);
        result->push_back(make_shared_ptr(new Constraint(make_named_values<Constraint>(
                            value_for<n::destination_type>(resolvent.destination_type()),
                            value_for<n::nothing_is_fine_too>(false),
                            value_for<n::reason>(reason),
                            value_for<n::spec>(*dep.spec().if_package()),
                            value_for<n::untaken>(si_untaken == interest),
                            value_for<n::use_existing>(_imp->fns.get_use_existing_fn()(
                                    resolvent, *dep.spec().if_package(), reason))
                            ))));
        return result;
    }
    else if (dep.spec().if_block())
        return _make_constraints_from_blocker(resolvent, *dep.spec().if_block(), reason);
    else
        throw InternalError(PALUDIS_HERE, "resolver bug: huh? it's not a block and it's not a package");
}

const std::tr1::shared_ptr<ConstraintSequence>
Decider::_make_constraints_from_blocker(
        const Resolvent &,
        const BlockDepSpec & spec,
        const std::tr1::shared_ptr<const Reason> & reason) const
{
    const std::tr1::shared_ptr<ConstraintSequence> result(new ConstraintSequence);

    DestinationTypes destination_types(_get_destination_types_for_blocker(spec));
    for (EnumIterator<DestinationType> t, t_end(last_dt) ; t != t_end ; ++t)
        if (destination_types[*t])
            result->push_back(make_shared_ptr(new Constraint(make_named_values<Constraint>(
                                value_for<n::destination_type>(*t),
                                value_for<n::nothing_is_fine_too>(true),
                                value_for<n::reason>(reason),
                                value_for<n::spec>(spec),
                                value_for<n::untaken>(false),
                                value_for<n::use_existing>(ue_if_possible)
                                ))));

    return result;
}

void
Decider::_apply_resolution_constraint(
        const Resolvent & resolvent,
        const std::tr1::shared_ptr<Resolution> & resolution,
        const std::tr1::shared_ptr<const Constraint> & constraint)
{
    if (resolution->decision())
        if (! _verify_new_constraint(resolvent, resolution, constraint))
            _made_wrong_decision(resolvent, resolution, constraint);

    resolution->constraints()->add(constraint);
}

namespace
{
    struct CheckConstraintVisitor
    {
        const Environment * const env;
        const Constraint constraint;

        CheckConstraintVisitor(const Environment * const e, const Constraint & c) :
            env(e),
            constraint(c)
        {
        }

        bool ok(const std::tr1::shared_ptr<const PackageID> & chosen_id) const
        {
            if (constraint.spec().if_package())
            {
                if (! match_package(*env, *constraint.spec().if_package(), *chosen_id, MatchPackageOptions()))
                    return false;
            }
            else
            {
                if (match_package(*env, constraint.spec().if_block()->blocking(),
                            *chosen_id, MatchPackageOptions()))
                    return false;
            }

            return true;
        }

        bool visit(const ChangesToMakeDecision & decision) const
        {
            return ok(decision.origin_id());
        }

        bool visit(const ExistingNoChangeDecision & decision) const
        {
            return ok(decision.existing_id());
        }

        bool visit(const NothingNoChangeDecision &) const
        {
            return constraint.nothing_is_fine_too();
        }

        bool visit(const UnableToMakeDecision &) const
        {
            return true;
        }

        bool visit(const RemoveDecision &) const
        {
            return constraint.nothing_is_fine_too();
        }
    };

    struct CheckUseExistingVisitor
    {
        const std::tr1::shared_ptr<const Constraint> constraint;

        CheckUseExistingVisitor(const std::tr1::shared_ptr<const Constraint> & c) :
            constraint(c)
        {
        }

        bool visit(const ExistingNoChangeDecision & decision) const
        {
            switch (constraint->use_existing())
            {
                case ue_if_possible:
                    break;

                case ue_only_if_transient:
                    if (! decision.is_transient())
                        return false;
                    break;

                case ue_if_same:
                    if (! decision.is_same())
                        return false;
                    break;

                case ue_if_same_version:
                    if (! decision.is_same_version())
                        return false;
                    break;

                case ue_never:
                case last_ue:
                    return false;
            }

            return true;
        }

        bool visit(const NothingNoChangeDecision &) const
        {
            return true;
        }

        bool visit(const UnableToMakeDecision &) const
        {
            return true;
        }

        bool visit(const ChangesToMakeDecision &) const
        {
            return true;
        }

        bool visit(const RemoveDecision &) const
        {
            return true;
        }
    };
}

bool
Decider::_check_constraint(const Resolvent &,
        const std::tr1::shared_ptr<const Constraint> & constraint,
        const std::tr1::shared_ptr<const Decision> & decision) const
{
    if (! decision->accept_returning<bool>(CheckConstraintVisitor(_imp->env, *constraint)))
        return false;

    if (! decision->accept_returning<bool>(CheckUseExistingVisitor(constraint)))
        return false;

    if (! constraint->untaken())
    {
        if (! decision->taken())
            return false;
    }

    return true;
}

bool
Decider::_verify_new_constraint(const Resolvent & resolvent,
        const std::tr1::shared_ptr<const Resolution> & resolution,
        const std::tr1::shared_ptr<const Constraint> & constraint)
{
    return _check_constraint(resolvent, constraint, resolution->decision());
}

namespace
{
    struct WrongDecisionVisitor
    {
        std::tr1::function<void ()> restart;

        WrongDecisionVisitor(const std::tr1::function<void ()> & r) :
            restart(r)
        {
        }

        void visit(const NothingNoChangeDecision &) const
        {
            /* going from nothing to something is fine */
        }

        void visit(const RemoveDecision &) const
        {
            restart();
        }

        void visit(const UnableToMakeDecision &) const
        {
            restart();
        }

        void visit(const ChangesToMakeDecision &) const
        {
            restart();
        }

        void visit(const ExistingNoChangeDecision &) const
        {
            restart();
        }
    };
}

void
Decider::_made_wrong_decision(const Resolvent & resolvent,
        const std::tr1::shared_ptr<Resolution> & resolution,
        const std::tr1::shared_ptr<const Constraint> & constraint)
{
    /* can we find a resolution that works for all our constraints? */
    std::tr1::shared_ptr<Resolution> adapted_resolution(make_shared_ptr(new Resolution(*resolution)));
    adapted_resolution->constraints()->add(constraint);

    const std::tr1::shared_ptr<Decision> decision(_try_to_find_decision_for(resolvent, adapted_resolution));
    if (decision)
    {
        resolution->decision()->accept(WrongDecisionVisitor(std::tr1::bind(
                        &Decider::_suggest_restart_with, this, resolvent, resolution, constraint, decision)));
        resolution->decision() = decision;
    }
    else
        resolution->decision() = _cannot_decide_for(resolvent, adapted_resolution);
}

void
Decider::_suggest_restart_with(const Resolvent & resolvent,
        const std::tr1::shared_ptr<const Resolution> & resolution,
        const std::tr1::shared_ptr<const Constraint> & constraint,
        const std::tr1::shared_ptr<const Decision> & decision) const
{
    throw SuggestRestart(resolvent, resolution->decision(), constraint, decision,
            _make_constraint_for_preloading(resolvent, decision, constraint));
}

const std::tr1::shared_ptr<const Constraint>
Decider::_make_constraint_for_preloading(
        const Resolvent &,
        const std::tr1::shared_ptr<const Decision> &,
        const std::tr1::shared_ptr<const Constraint> & c) const
{
    const std::tr1::shared_ptr<Constraint> result(new Constraint(*c));

    const std::tr1::shared_ptr<PresetReason> reason(new PresetReason("restarted because of ", c->reason()));
    result->reason() = reason;

    if (result->spec().if_package())
    {
        PackageDepSpec s(_make_spec_for_preloading(*result->spec().if_package()));
        result->spec().if_package() = make_shared_ptr(new PackageDepSpec(s));
    }
    else
    {
        PackageDepSpec s(_make_spec_for_preloading(result->spec().if_block()->blocking()));
        result->spec().if_block() = make_shared_ptr(new BlockDepSpec(
                    "!" + stringify(s),
                    s,
                    result->spec().if_block()->strong()));
    }

    return result;
}

const PackageDepSpec
Decider::_make_spec_for_preloading(const PackageDepSpec & spec) const
{
    PartiallyMadePackageDepSpec result(spec);

    /* we don't want to copy use deps from the constraint, since things like
     * [foo?] start to get weird when there's no longer an associated ID. */
    result.clear_additional_requirements();

    return result;
}

void
Decider::_decide(const Resolvent & resolvent, const std::tr1::shared_ptr<Resolution> & resolution)
{
    Context context("When deciding upon an origin ID to use for '" + stringify(resolvent) + "':");

    std::tr1::shared_ptr<Decision> decision(_try_to_find_decision_for(resolvent, resolution));
    if (decision)
        resolution->decision() = decision;
    else
        resolution->decision() = _cannot_decide_for(resolvent, resolution);
}

namespace
{
    struct DependenciesNecessityVisitor
    {
        const std::tr1::shared_ptr<const PackageID> visit(const NothingNoChangeDecision &) const
        {
            return make_null_shared_ptr();
        }

        const std::tr1::shared_ptr<const PackageID> visit(const RemoveDecision &) const
        {
            return make_null_shared_ptr();
        }

        const std::tr1::shared_ptr<const PackageID> visit(const UnableToMakeDecision &) const
        {
            return make_null_shared_ptr();
        }

        const std::tr1::shared_ptr<const PackageID> visit(const ExistingNoChangeDecision & decision) const
        {
            if (decision.taken())
                return decision.existing_id();
            else
                return make_null_shared_ptr();
        }

        const std::tr1::shared_ptr<const PackageID> visit(const ChangesToMakeDecision & decision) const
        {
            if (decision.taken())
                return decision.origin_id();
            else
                return make_null_shared_ptr();
        }
    };
}

void
Decider::_add_dependencies_if_necessary(
        const Resolvent & our_resolvent,
        const std::tr1::shared_ptr<Resolution> & our_resolution)
{
    const std::tr1::shared_ptr<const PackageID> package_id(
            our_resolution->decision()->accept_returning<std::tr1::shared_ptr<const PackageID> >(
                DependenciesNecessityVisitor()));
    if (! package_id)
        return;

    Context context("When adding dependencies for '" + stringify(our_resolvent) + "' with '"
            + stringify(*package_id) + "':");

    const std::tr1::shared_ptr<SanitisedDependencies> deps(new SanitisedDependencies);
    deps->populate(*this, our_resolvent, package_id);

    for (SanitisedDependencies::ConstIterator s(deps->begin()), s_end(deps->end()) ;
            s != s_end ; ++s)
    {
        Context context_2("When handling dependency '" + stringify(s->spec()) + "':");

        SpecInterest interest(_interest_in_spec(our_resolvent, our_resolution, *s));

        switch (interest)
        {
            case si_ignore:
                continue;

            case si_untaken:
            case si_take:
            case last_si:
                break;
        }

        const std::tr1::shared_ptr<DependencyReason> reason(new DependencyReason(
                    package_id, our_resolvent, *s, _already_met(*s)));

        std::tr1::shared_ptr<const Resolvents> resolvents;

        if (s->spec().if_package())
            resolvents = _get_resolvents_for(*s->spec().if_package(), reason);
        else
            resolvents = _get_resolvents_for_blocker(*s->spec().if_block());

        if (resolvents->empty())
        {
            if (s->spec().if_package())
                resolvents = _get_error_resolvents_for(*s->spec().if_package(), reason);
            else
            {
                /* blocking on something that doesn't exist is fine */
            }
        }

        for (Resolvents::ConstIterator r(resolvents->begin()), r_end(resolvents->end()) ;
                r != r_end ; ++r)
        {
            const std::tr1::shared_ptr<Resolution> dep_resolution(_resolution_for_resolvent(*r, true));
            const std::tr1::shared_ptr<ConstraintSequence> constraints(_make_constraints_from_dependency(
                        our_resolvent, *s, reason, interest));

            for (ConstraintSequence::ConstIterator c(constraints->begin()), c_end(constraints->end()) ;
                    c != c_end ; ++c)
                _apply_resolution_constraint(*r, dep_resolution, *c);
        }
    }
}

SpecInterest
Decider::_interest_in_spec(const Resolvent & resolvent,
        const std::tr1::shared_ptr<const Resolution> & resolution, const SanitisedDependency & dep) const
{
    return _imp->fns.interest_in_spec_fn()(resolvent, resolution, dep);
}

const std::tr1::shared_ptr<Constraints>
Decider::_initial_constraints_for(const Resolvent & r) const
{
    return _imp->fns.get_initial_constraints_for_fn()(r);
}

namespace
{
    struct ChosenIDVisitor
    {
        const std::tr1::shared_ptr<const PackageID> visit(const ChangesToMakeDecision & decision) const
        {
            return decision.origin_id();
        }

        const std::tr1::shared_ptr<const PackageID> visit(const ExistingNoChangeDecision & decision) const
        {
            return decision.existing_id();
        }

        const std::tr1::shared_ptr<const PackageID> visit(const NothingNoChangeDecision &) const
        {
            return make_null_shared_ptr();
        }

        const std::tr1::shared_ptr<const PackageID> visit(const UnableToMakeDecision &) const
        {
            return make_null_shared_ptr();
        }

        const std::tr1::shared_ptr<const PackageID> visit(const RemoveDecision &) const
        {
            return make_null_shared_ptr();
        }
    };
}

std::pair<AnyChildScore, OperatorScore>
Decider::find_any_score(const Resolvent & our_resolvent, const SanitisedDependency & dep) const
{
    Context context("When working out whether we'd like '" + stringify(dep.spec()) + "' because of '"
            + stringify(our_resolvent) + "':");

    if (dep.spec().if_block())
        throw InternalError(PALUDIS_HERE, "unimplemented: blockers inside || blocks are horrid");

    const PackageDepSpec & spec(*dep.spec().if_package());

    // note: make sure the worst_score declaration in
    // AnyDepSpecChildHandler::commit in satitised_dependencies.cc
    // matches this logic
    OperatorScore operator_bias(os_worse_than_worst);
    if (spec.version_requirements_ptr() && ! spec.version_requirements_ptr()->empty())
    {
        OperatorScore score(os_worse_than_worst);
        for (VersionRequirements::ConstIterator v(spec.version_requirements_ptr()->begin()),
                v_end(spec.version_requirements_ptr()->end()) ;
                v != v_end ; ++v)
        {
            OperatorScore local_score(os_worse_than_worst);

            switch (v->version_operator().value())
            {
                case vo_greater:
                case vo_greater_equal:
                    local_score = os_greater_or_none;
                    break;

                case vo_equal:
                case vo_tilde:
                case vo_nice_equal_star:
                case vo_stupid_equal_star:
                case vo_tilde_greater:
                    local_score = os_equal;
                    break;

                case vo_less_equal:
                case vo_less:
                    local_score = os_less;
                    break;

                case last_vo:
                    local_score = os_less;
                    break;
            }

            if (score == os_worse_than_worst)
                score = local_score;
            else
                switch (spec.version_requirements_mode())
                {
                    case vr_and:
                        score = std::min(score, local_score);
                        break;

                    case vr_or:
                        score = std::max(score, local_score);
                        break;

                    case last_vr:
                        break;
                }
        }
        operator_bias = score;
    }
    else
    {
        /* don't bias no operator over a >= operator, so || ( >=foo-2 bar )
         * still likes foo. */
        operator_bias = os_greater_or_none;
    }

    /* explicit preferences come first */
    if (spec.package_ptr())
    {
        Tribool prefer_or_avoid(_imp->fns.prefer_or_avoid_fn()(*spec.package_ptr()));
        if (prefer_or_avoid.is_true())
            return std::make_pair(acs_prefer, operator_bias);
        else if (prefer_or_avoid.is_false())
            return std::make_pair(acs_avoid, operator_bias);
    }

    /* best: already installed */
    {
        const std::tr1::shared_ptr<const PackageIDSequence> installed_ids((*_imp->env)[selection::BestVersionOnly(
                    generator::Matches(spec, MatchPackageOptions()) |
                    filter::InstalledAtRoot(FSEntry("/")))]);
        if (! installed_ids->empty())
            return std::make_pair(acs_already_installed, operator_bias);
    }

    /* next: already installed, except with the wrong options */
    if (spec.additional_requirements_ptr())
    {
        const std::tr1::shared_ptr<const PackageIDSequence> installed_ids((*_imp->env)[selection::BestVersionOnly(
                    generator::Matches(spec, MatchPackageOptions() + mpo_ignore_additional_requirements) |
                    filter::InstalledAtRoot(FSEntry("/")))]);
        if (! installed_ids->empty())
            return std::make_pair(acs_wrong_options_installed, operator_bias);
    }

    const std::tr1::shared_ptr<const PackageID> id(resolution_for_resolvent(
                our_resolvent)->decision()->accept_returning<std::tr1::shared_ptr<const PackageID> >(
                    ChosenIDVisitor()));
    if (! id)
        throw InternalError(PALUDIS_HERE, "resolver bug: why don't we have an id?");

    const std::tr1::shared_ptr<DependencyReason> reason(new DependencyReason(id, our_resolvent, dep, _already_met(dep)));
    const std::tr1::shared_ptr<const Resolvents> resolvents(_get_resolvents_for(spec, reason));

    /* next: will already be installing */
    {
        for (Resolvents::ConstIterator r(resolvents->begin()), r_end(resolvents->end()) ;
                r != r_end ; ++r)
        {
            ResolutionsByResolventMap::const_iterator i(_imp->resolutions_by_resolvent.find(*r));
            if (i != _imp->resolutions_by_resolvent.end())
                return std::make_pair(acs_will_be_installing, operator_bias);
        }
    }

    /* next: could install */
    {
        for (Resolvents::ConstIterator r(resolvents->begin()), r_end(resolvents->end()) ;
                r != r_end ; ++r)
        {
            const std::tr1::shared_ptr<Resolution> resolution(_create_resolution_for_resolvent(*r));
            const std::tr1::shared_ptr<ConstraintSequence> constraints(_make_constraints_from_dependency(
                        our_resolvent, dep, reason, si_take));
            for (ConstraintSequence::ConstIterator c(constraints->begin()), c_end(constraints->end()) ;
                    c != c_end ; ++c)
                resolution->constraints()->add(*c);
            const std::tr1::shared_ptr<Decision> decision(_try_to_find_decision_for(*r, resolution));
            if (decision)
                return std::make_pair(acs_could_install, operator_bias);
        }
    }

    /* next: exists */
    {
        const std::tr1::shared_ptr<const PackageIDSequence> ids((*_imp->env)[selection::BestVersionOnly(
                    generator::Matches(spec, MatchPackageOptions() + mpo_ignore_additional_requirements)
                    )]);
        if (! ids->empty())
            return std::make_pair(acs_exists, operator_bias);
    }

    /* yay, people are depping upon packages that don't exist again. I SMELL A LESSPIPE. */
    return std::make_pair(acs_hate_hate_hate, operator_bias);
}

namespace
{
    struct SlotNameFinder
    {
        std::tr1::shared_ptr<SlotName> visit(const SlotExactRequirement & s)
        {
            return make_shared_ptr(new SlotName(s.slot()));
        }

        std::tr1::shared_ptr<SlotName> visit(const SlotAnyUnlockedRequirement &)
        {
            return make_null_shared_ptr();
        }

        std::tr1::shared_ptr<SlotName> visit(const SlotAnyLockedRequirement &)
        {
            return make_null_shared_ptr();
        }
    };
}

const std::tr1::shared_ptr<const Resolvents>
Decider::_get_resolvents_for_blocker(const BlockDepSpec & spec) const
{
    Context context("When finding slots for '" + stringify(spec) + "':");

    std::tr1::shared_ptr<SlotName> exact_slot;
    if (spec.blocking().slot_requirement_ptr())
    {
        SlotNameFinder f;
        exact_slot = spec.blocking().slot_requirement_ptr()->accept_returning<std::tr1::shared_ptr<SlotName> >(f);
    }

    DestinationTypes destination_types(_get_destination_types_for_blocker(spec));
    std::tr1::shared_ptr<Resolvents> result(new Resolvents);
    if (exact_slot)
    {
        for (EnumIterator<DestinationType> t, t_end(last_dt) ; t != t_end ; ++t)
            if (destination_types[*t])
                result->push_back(Resolvent(spec.blocking(), *exact_slot, *t));
    }
    else
    {
        const std::tr1::shared_ptr<const PackageIDSequence> ids((*_imp->env)[selection::BestVersionInEachSlot(
                    generator::Package(*spec.blocking().package_ptr())
                    )]);
        for (PackageIDSequence::ConstIterator i(ids->begin()), i_end(ids->end()) ;
                i != i_end ; ++i)
            for (EnumIterator<DestinationType> t, t_end(last_dt) ; t != t_end ; ++t)
                if (destination_types[*t])
                    result->push_back(Resolvent(*i, *t));
    }

    return result;
}

const DestinationTypes
Decider::_get_destination_types_for_blocker(const BlockDepSpec &) const
{
    return DestinationTypes() + dt_install_to_slash;
}

const std::tr1::shared_ptr<const Resolvents>
Decider::_get_resolvents_for(
        const PackageDepSpec & spec,
        const std::tr1::shared_ptr<const Reason> & reason) const
{
    Context context("When finding slots for '" + stringify(spec) + "':");

    std::tr1::shared_ptr<SlotName> exact_slot;

    if (spec.slot_requirement_ptr())
    {
        SlotNameFinder f;
        exact_slot = spec.slot_requirement_ptr()->accept_returning<std::tr1::shared_ptr<SlotName> >(f);
    }

    return _imp->fns.get_resolvents_for_fn()(spec, exact_slot, reason);
}

const DestinationTypes
Decider::_get_destination_types_for(
        const PackageDepSpec & spec,
        const std::tr1::shared_ptr<const PackageID> & id,
        const std::tr1::shared_ptr<const Reason> & reason) const
{
    return _imp->fns.get_destination_types_for_fn()(spec, id, reason);
}

const std::tr1::shared_ptr<const Resolvents>
Decider::_get_error_resolvents_for(
        const PackageDepSpec & spec,
        const std::tr1::shared_ptr<const Reason> & reason) const
{
    Context context("When finding slots for '" + stringify(spec) + "', which can't be found the normal way:");

    std::tr1::shared_ptr<Resolvents> result(new Resolvents);
    DestinationTypes destination_types(_get_destination_types_for(spec, make_null_shared_ptr(), reason));
    for (EnumIterator<DestinationType> t, t_end(last_dt) ; t != t_end ; ++t)
        if (destination_types[*t])
            result->push_back(Resolvent(spec, true, *t));
    return result;
}

const std::tr1::shared_ptr<Decision>
Decider::_try_to_find_decision_for(
        const Resolvent & resolvent,
        const std::tr1::shared_ptr<const Resolution> & resolution) const
{
    const std::tr1::shared_ptr<const PackageID> existing_id(_find_existing_id_for(resolvent, resolution));
    std::pair<const std::tr1::shared_ptr<const PackageID>, bool> installable_id_best(_find_installable_id_for(resolvent, resolution));
    const std::tr1::shared_ptr<const PackageID> installable_id(installable_id_best.first);
    bool best(installable_id_best.second);

    if (resolution->constraints()->nothing_is_fine_too())
    {
        const std::tr1::shared_ptr<const PackageIDSequence> existing_resolvent_ids(_installed_ids(resolvent));
        if (existing_resolvent_ids->empty())
        {
            /* nothing existing, but nothing's ok */
            return make_shared_ptr(new NothingNoChangeDecision(
                        ! resolution->constraints()->all_untaken()
                        ));
        }
    }

    if (installable_id && ! existing_id)
    {
        /* there's nothing suitable existing. we fix the last_ct when we do
         * destinations. */
        return make_shared_ptr(new ChangesToMakeDecision(
                    installable_id,
                    best,
                    last_ct,
                    ! resolution->constraints()->all_untaken(),
                    make_null_shared_ptr()
                    ));
    }
    else if (existing_id && ! installable_id)
    {
        /* there's nothing installable. this may or may not be ok. */
        bool is_transient(existing_id->transient_key() && existing_id->transient_key()->value());

        switch (resolution->constraints()->strictest_use_existing())
        {
            case ue_if_possible:
                break;

            case ue_only_if_transient:
            case ue_if_same:
            case ue_if_same_version:
                if (! is_transient)
                    return make_null_shared_ptr();
                break;

            case ue_never:
                return make_null_shared_ptr();

            case last_ue:
                break;
        }

        return make_shared_ptr(new ExistingNoChangeDecision(
                    existing_id,
                    true,
                    true,
                    is_transient,
                    ! resolution->constraints()->all_untaken()
                    ));
    }
    else if ((! existing_id) && (! installable_id))
    {
        /* we can't stick with our existing id, if there is one, and we can't
         * fix it by installing things. this might be an error, or we might be
         * able to remove things. */
        if (resolution->constraints()->nothing_is_fine_too() && _installed_but_allowed_to_remove(resolvent))
            return make_shared_ptr(new RemoveDecision(
                        _installed_ids(resolvent),
                        ! resolution->constraints()->all_untaken()
                        ));
        else
            return make_null_shared_ptr();
    }
    else if (existing_id && installable_id)
    {
        bool is_same_version(existing_id->version() == installable_id->version());
        bool is_same(false);

        if (is_same_version)
        {
            is_same = true;

            std::set<ChoiceNameWithPrefix> common;
            if (existing_id->choices_key() && installable_id->choices_key())
            {
                std::set<ChoiceNameWithPrefix> i_common, u_common;
                for (Choices::ConstIterator k(installable_id->choices_key()->value()->begin()),
                        k_end(installable_id->choices_key()->value()->end()) ;
                        k != k_end ; ++k)
                {
                    if (! (*k)->consider_added_or_changed())
                        continue;

                    for (Choice::ConstIterator i((*k)->begin()), i_end((*k)->end()) ;
                            i != i_end ; ++i)
                        if ((*i)->explicitly_listed())
                            i_common.insert((*i)->name_with_prefix());
                }

                for (Choices::ConstIterator k(existing_id->choices_key()->value()->begin()),
                        k_end(existing_id->choices_key()->value()->end()) ;
                        k != k_end ; ++k)
                {
                    if (! (*k)->consider_added_or_changed())
                        continue;

                    for (Choice::ConstIterator i((*k)->begin()), i_end((*k)->end()) ;
                            i != i_end ; ++i)
                        if ((*i)->explicitly_listed())
                            u_common.insert((*i)->name_with_prefix());
                }

                std::set_intersection(
                        i_common.begin(), i_common.end(),
                        u_common.begin(), u_common.end(),
                        std::inserter(common, common.begin()));
            }

            for (std::set<ChoiceNameWithPrefix>::const_iterator f(common.begin()), f_end(common.end()) ;
                    f != f_end ; ++f)
                if (installable_id->choices_key()->value()->find_by_name_with_prefix(*f)->enabled() !=
                        existing_id->choices_key()->value()->find_by_name_with_prefix(*f)->enabled())
                {
                    is_same = false;
                    break;
                }
        }

        bool is_transient(existing_id->transient_key() && existing_id->transient_key()->value());

        /* we've got existing and installable. do we have any reason not to pick the existing id? */
        const std::tr1::shared_ptr<Decision> existing(new ExistingNoChangeDecision(
                    existing_id,
                    is_same,
                    is_same_version,
                    is_transient,
                    ! resolution->constraints()->all_untaken()
                    ));
        const std::tr1::shared_ptr<Decision> changes_to_make(new ChangesToMakeDecision(
                    installable_id,
                    best,
                    last_ct,
                    ! resolution->constraints()->all_untaken(),
                    make_null_shared_ptr()
                    ));

        switch (resolution->constraints()->strictest_use_existing())
        {
            case ue_only_if_transient:
            case ue_never:
                return make_shared_ptr(new ChangesToMakeDecision(
                            installable_id,
                            best,
                            last_ct,
                            ! resolution->constraints()->all_untaken(),
                            make_null_shared_ptr()
                            ));

            case ue_if_same:
                if (is_same)
                    return existing;
                else
                    return changes_to_make;

            case ue_if_same_version:
                if (is_same_version)
                    return existing;
                else
                    return changes_to_make;

            case ue_if_possible:
                return existing;

            case last_ue:
                break;
        }
    }

    throw InternalError(PALUDIS_HERE, "resolver bug: shouldn't be reached");
}

const std::tr1::shared_ptr<Decision>
Decider::_cannot_decide_for(
        const Resolvent & resolvent,
        const std::tr1::shared_ptr<const Resolution> & resolution) const
{
    const std::tr1::shared_ptr<UnsuitableCandidates> unsuitable_candidates(new UnsuitableCandidates);

    const std::tr1::shared_ptr<const PackageID> existing_id(_find_existing_id_for(resolvent, resolution));
    if (existing_id)
        unsuitable_candidates->push_back(_make_unsuitable_candidate(resolvent, resolution, existing_id, true));

    const std::tr1::shared_ptr<const PackageIDSequence> installable_ids(_find_installable_id_candidates_for(resolvent, resolution, true));
    for (PackageIDSequence::ConstIterator i(installable_ids->begin()), i_end(installable_ids->end()) ;
            i != i_end ; ++i)
        unsuitable_candidates->push_back(_make_unsuitable_candidate(resolvent, resolution, *i, false));

    return make_shared_ptr(new UnableToMakeDecision(
                unsuitable_candidates,
                ! resolution->constraints()->all_untaken()
                ));
}

UnsuitableCandidate
Decider::_make_unsuitable_candidate(
        const Resolvent & resolvent,
        const std::tr1::shared_ptr<const Resolution> &,
        const std::tr1::shared_ptr<const PackageID> & id,
        const bool existing) const
{
    return make_named_values<UnsuitableCandidate>(
            value_for<n::package_id>(id),
            value_for<n::unmet_constraints>(_get_unmatching_constraints(resolvent, id, existing))
            );
}

const std::tr1::shared_ptr<const PackageID>
Decider::_find_existing_id_for(const Resolvent & resolvent, const std::tr1::shared_ptr<const Resolution> & resolution) const
{
    const std::tr1::shared_ptr<const PackageIDSequence> ids(_installed_ids(resolvent));
    return _find_id_for_from(resolvent, resolution, ids).first;
}

bool
Decider::_installed_but_allowed_to_remove(const Resolvent & resolvent) const
{
    const std::tr1::shared_ptr<const PackageIDSequence> ids(_installed_ids(resolvent));
    if (ids->empty())
        return false;

    return ids->end() == std::find_if(ids->begin(), ids->end(),
            std::tr1::bind(std::logical_not<bool>(), std::tr1::bind(&Decider::_allowed_to_remove, this, std::tr1::placeholders::_1)));
}

bool
Decider::_allowed_to_remove(const std::tr1::shared_ptr<const PackageID> & id) const
{
    return id->supports_action(SupportsActionTest<UninstallAction>()) && _imp->fns.allowed_to_remove_fn()(id);
}

bool
Decider::_allowed_to_break(const std::tr1::shared_ptr<const PackageID> & id) const
{
    return _imp->fns.allowed_to_break_fn()(id);
}

bool
Decider::_remove_if_dependent(const std::tr1::shared_ptr<const PackageID> & id) const
{
    return _imp->fns.remove_if_dependent_fn()(id);
}

const std::tr1::shared_ptr<const PackageIDSequence>
Decider::_installed_ids(const Resolvent & resolvent) const
{
    return (*_imp->env)[selection::AllVersionsSorted(
            _make_destination_filtered_generator(generator::Package(resolvent.package()), resolvent) |
            make_slot_filter(resolvent)
            )];
}

const std::tr1::shared_ptr<const PackageIDSequence>
Decider::_find_installable_id_candidates_for(const Resolvent & resolvent,
        const std::tr1::shared_ptr<const Resolution> &,
        const bool include_errors) const
{
    return (*_imp->env)[selection::AllVersionsSorted(
            generator::Package(resolvent.package()) |
            make_slot_filter(resolvent) |
            filter::SupportsAction<InstallAction>() |
            ((! include_errors) ? Filter(filter::NotMasked()) : Filter(filter::All()))
            )];
}

const std::pair<const std::tr1::shared_ptr<const PackageID>, bool>
Decider::_find_installable_id_for(const Resolvent & resolvent, const std::tr1::shared_ptr<const Resolution> & resolution) const
{
    return _find_id_for_from(resolvent, resolution, _find_installable_id_candidates_for(resolvent, resolution, false));
}

const std::pair<const std::tr1::shared_ptr<const PackageID>, bool>
Decider::_find_id_for_from(
        const Resolvent &, const std::tr1::shared_ptr<const Resolution> & resolution,
        const std::tr1::shared_ptr<const PackageIDSequence> & ids) const
{
    bool best(true);
    for (PackageIDSequence::ReverseConstIterator i(ids->rbegin()), i_end(ids->rend()) ;
            i != i_end ; ++i)
    {
        bool ok(true);
        for (Constraints::ConstIterator c(resolution->constraints()->begin()),
                c_end(resolution->constraints()->end()) ;
                c != c_end ; ++c)
        {
            if ((*c)->spec().if_package())
                ok = ok && match_package(*_imp->env, *(*c)->spec().if_package(), **i, MatchPackageOptions());
            else
                ok = ok && ! match_package(*_imp->env, (*c)->spec().if_block()->blocking(), **i, MatchPackageOptions());

            if (! ok)
                break;
        }

        if (ok)
            return std::make_pair(*i, best);

        best = false;
    }

    return std::make_pair(make_null_shared_ptr(), false);
}

const std::tr1::shared_ptr<const Constraints>
Decider::_get_unmatching_constraints(
        const Resolvent & resolvent,
        const std::tr1::shared_ptr<const PackageID> & id,
        const bool existing) const
{
    const std::tr1::shared_ptr<const Resolution> resolution(resolution_for_resolvent(resolvent));
    const std::tr1::shared_ptr<Constraints> result(new Constraints);

    for (Constraints::ConstIterator c(resolution->constraints()->begin()),
            c_end(resolution->constraints()->end()) ;
            c != c_end ; ++c)
    {
        std::tr1::shared_ptr<Decision> decision;

        if (existing)
        {
            bool is_transient(id->transient_key() && id->transient_key()->value());
            decision.reset(new ExistingNoChangeDecision(
                    id,
                    true,
                    true,
                    is_transient,
                    ! (*c)->untaken()
                    ));
        }
        else
            decision.reset(new ChangesToMakeDecision(
                        id,
                        false,
                        last_ct,
                        ! (*c)->untaken(),
                        make_null_shared_ptr()
                        ));
        if (! _check_constraint(resolvent, *c, decision))
            result->add(*c);
    }

    return result;
}

const std::tr1::shared_ptr<const RewrittenSpec>
Decider::rewrite_if_special(
        const PackageOrBlockDepSpec & spec,
        const std::tr1::shared_ptr<const Resolvent> & maybe_from) const
{
    return _imp->rewriter.rewrite_if_special(spec, maybe_from);
}

void
Decider::add_target_with_reason(const PackageOrBlockDepSpec & spec, const std::tr1::shared_ptr<const Reason> & reason)
{
    Context context("When adding target '" + stringify(spec) + "':");

    _imp->env->trigger_notifier_callback(NotifierCallbackResolverStepEvent());

    const std::tr1::shared_ptr<const RewrittenSpec> if_rewritten(rewrite_if_special(spec, make_null_shared_ptr()));
    if (if_rewritten)
    {
        for (Sequence<PackageOrBlockDepSpec>::ConstIterator i(if_rewritten->specs()->begin()), i_end(if_rewritten->specs()->end()) ;
                i != i_end ; ++i)
            if (i->if_package())
                add_target_with_reason(*i->if_package(), reason);
            else
                throw InternalError(PALUDIS_HERE, "resolver bug: rewritten " + stringify(spec) + " includes " + stringify(*i));
    }
    else
    {
        PackageDepSpec base_spec(spec.if_package() ? *spec.if_package() : spec.if_block()->blocking());
        std::tr1::shared_ptr<const Resolvents> resolvents(_get_resolvents_for(base_spec, reason));
        if (resolvents->empty())
        {
            if (spec.if_package())
                resolvents = _get_error_resolvents_for(*spec.if_package(), reason);
            else
            {
                /* blocking on something that doesn't exist is fine */
            }
        }

        for (Resolvents::ConstIterator r(resolvents->begin()), r_end(resolvents->end()) ;
                r != r_end ; ++r)
        {
            Context context_2("When adding constraints from target '" + stringify(spec) + "' to resolvent '"
                    + stringify(*r) + "':");

            const std::tr1::shared_ptr<Resolution> dep_resolution(_resolution_for_resolvent(*r, true));
            const std::tr1::shared_ptr<ConstraintSequence> constraints(_make_constraints_from_target(*r, spec, reason));

            for (ConstraintSequence::ConstIterator c(constraints->begin()), c_end(constraints->end()) ;
                    c != c_end ; ++c)
                _apply_resolution_constraint(*r, dep_resolution, *c);
        }
    }
}

void
Decider::resolve()
{
    while (true)
    {
        _imp->env->trigger_notifier_callback(NotifierCallbackResolverStageEvent("Deciding"));
        _resolve_decide_with_dependencies();
        _imp->env->trigger_notifier_callback(NotifierCallbackResolverStageEvent("Finding Dependents"));
        if (! _resolve_dependents())
            break;
    }

    _imp->env->trigger_notifier_callback(NotifierCallbackResolverStageEvent("Finding Destinations"));
    _resolve_destinations();
}

bool
Decider::_already_met(const SanitisedDependency & dep) const
{
    const std::tr1::shared_ptr<const PackageIDSequence> installed_ids((*_imp->env)[selection::BestVersionOnly(
                generator::Matches(dep.spec().if_package() ?
                    *dep.spec().if_package() :
                    dep.spec().if_block()->blocking(),
                    MatchPackageOptions()) |
                filter::InstalledAtRoot(FSEntry("/")))]);
    if (installed_ids->empty())
        return dep.spec().if_block();
    else
        return dep.spec().if_package();
}

