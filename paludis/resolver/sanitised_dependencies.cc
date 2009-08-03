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

#include <paludis/resolver/sanitised_dependencies.hh>
#include <paludis/resolver/resolver.hh>
#include <paludis/resolver/qpn_s.hh>
#include <paludis/util/make_named_values.hh>
#include <paludis/util/save.hh>
#include <paludis/util/stringify.hh>
#include <paludis/util/options.hh>
#include <paludis/util/join.hh>
#include <paludis/spec_tree.hh>
#include <paludis/package_dep_spec_properties.hh>
#include <paludis/slot_requirement.hh>
#include <paludis/metadata_key.hh>
#include <paludis/package_id.hh>
#include <set>
#include <list>

using namespace paludis;
using namespace paludis::resolver;

namespace
{
    struct IsUnrestrictedSlot
    {
        bool visit(const SlotExactRequirement &) const
        {
            return false;
        }

        bool visit(const SlotAnyUnlockedRequirement &) const
        {
            return true;
        }

        bool visit(const SlotAnyLockedRequirement &) const
        {
            return true;
        }
    };

    template <typename T_>
    void list_push_back(std::list<T_> * const l, const T_ & t)
    {
        l->push_back(t);
    }

    struct AnyDepSpecChildHandler
    {
        const Resolver & resolver;
        const QPN_S our_qpn_s;
        const std::tr1::function<SanitisedDependency (const PackageDepSpec &)> parent_make_sanitised;

        bool super_complicated, nested;

        std::list<std::list<PackageDepSpec> > child_groups;
        std::list<PackageDepSpec> * active_sublist;
        std::set<QualifiedPackageName> seen_qpns;

        bool seen_unadorned, seen_slots, seen_additional_requirements, seen_version_requirements, seen_unknown;

        AnyDepSpecChildHandler(const Resolver & r, const QPN_S & q,
                const std::tr1::function<SanitisedDependency (const PackageDepSpec &)> & f) :
            resolver(r),
            our_qpn_s(q),
            parent_make_sanitised(f),
            super_complicated(false),
            nested(false),
            active_sublist(0),
            seen_unadorned(false),
            seen_slots(false),
            seen_additional_requirements(false),
            seen_version_requirements(false),
            seen_unknown(false)
        {
        }

        void visit_spec(const PackageDepSpec & spec)
        {
            if (spec.package_ptr())
            {
                seen_qpns.insert(*spec.package_ptr());

                if (active_sublist)
                    active_sublist->push_back(spec);
                else
                {
                    std::list<PackageDepSpec> l;
                    l.push_back(spec);
                    child_groups.push_back(l);
                }

                if (spec.slot_requirement_ptr())
                    seen_slots = true;

                if (spec.additional_requirements_ptr() && ! spec.additional_requirements_ptr()->empty())
                    seen_additional_requirements = true;

                if (spec.version_requirements_ptr() && ! spec.version_requirements_ptr()->empty())
                    seen_version_requirements = true;

                if (package_dep_spec_has_properties(spec, make_named_values<PackageDepSpecProperties>(
                                value_for<n::has_additional_requirements>(false),
                                value_for<n::has_category_name_part>(false),
                                value_for<n::has_from_repository>(false),
                                value_for<n::has_in_repository>(false),
                                value_for<n::has_installable_to_path>(false),
                                value_for<n::has_installable_to_repository>(false),
                                value_for<n::has_installed_at_path>(false),
                                value_for<n::has_package>(indeterminate),
                                value_for<n::has_package_name_part>(false),
                                value_for<n::has_slot_requirement>(false),
                                value_for<n::has_tag>(indeterminate),
                                value_for<n::has_version_requirements>(false)
                                )))
                    seen_unadorned = true;
                else if (! package_dep_spec_has_properties(spec, make_named_values<PackageDepSpecProperties>(
                                value_for<n::has_additional_requirements>(indeterminate),
                                value_for<n::has_category_name_part>(false),
                                value_for<n::has_from_repository>(false),
                                value_for<n::has_in_repository>(false),
                                value_for<n::has_installable_to_path>(false),
                                value_for<n::has_installable_to_repository>(false),
                                value_for<n::has_installed_at_path>(false),
                                value_for<n::has_package>(indeterminate),
                                value_for<n::has_package_name_part>(false),
                                value_for<n::has_slot_requirement>(indeterminate),
                                value_for<n::has_tag>(indeterminate),
                                value_for<n::has_version_requirements>(indeterminate)
                                )))
                    seen_unknown = true;
            }
            else
                super_complicated = true;
        }

        void visit(const DependencySpecTree::NodeType<PackageDepSpec>::Type & node)
        {
            visit_spec(*node.spec());
        }

        void visit(const DependencySpecTree::NodeType<BlockDepSpec>::Type &)
        {
            super_complicated = true;
        }

        void visit(const DependencySpecTree::NodeType<ConditionalDepSpec>::Type & node)
        {
            if (node.spec()->condition_met())
            {
                nested = true;

                if (active_sublist)
                    std::for_each(indirect_iterator(node.begin()), indirect_iterator(node.end()), accept_visitor(*this));
                else
                {
                    Save<std::list<PackageDepSpec> *> save_active_sublist(&active_sublist, 0);
                    active_sublist = &*child_groups.insert(child_groups.end(), std::list<PackageDepSpec>());
                    std::for_each(indirect_iterator(node.begin()), indirect_iterator(node.end()), accept_visitor(*this));
                }
            }
        }

        void visit(const DependencySpecTree::NodeType<AllDepSpec>::Type & node)
        {
            nested = true;

            if (active_sublist)
                std::for_each(indirect_iterator(node.begin()), indirect_iterator(node.end()), accept_visitor(*this));
            else
            {
                Save<std::list<PackageDepSpec> *> save_active_sublist(&active_sublist, 0);
                active_sublist = &*child_groups.insert(child_groups.end(), std::list<PackageDepSpec>());
                std::for_each(indirect_iterator(node.begin()), indirect_iterator(node.end()), accept_visitor(*this));
            }
        }

        void visit(const DependencySpecTree::NodeType<AnyDepSpec>::Type & node)
        {
            AnyDepSpecChildHandler h(resolver, our_qpn_s, parent_make_sanitised);
            std::for_each(indirect_iterator(node.begin()), indirect_iterator(node.end()), accept_visitor(h));
            std::list<SanitisedDependency> l;
            h.commit(
                    parent_make_sanitised,
                    std::tr1::bind(&list_push_back<SanitisedDependency>, &l, std::tr1::placeholders::_1)
                    );

            if (active_sublist)
            {
                for (std::list<SanitisedDependency>::const_iterator i(l.begin()), i_end(l.end()) ;
                        i != i_end ; ++i)
                    visit_spec(i->spec());
            }
            else
            {
                Save<std::list<PackageDepSpec> *> save_active_sublist(&active_sublist, 0);
                active_sublist = &*child_groups.insert(child_groups.end(), std::list<PackageDepSpec>());
                for (std::list<SanitisedDependency>::const_iterator i(l.begin()), i_end(l.end()) ;
                        i != i_end ; ++i)
                    visit_spec(i->spec());
            }
        }

        void visit(const DependencySpecTree::NodeType<NamedSetDepSpec>::Type &)
        {
            super_complicated = true;
        }

        void visit(const DependencySpecTree::NodeType<DependencyLabelsDepSpec>::Type &)
        {
            super_complicated = true;
        }

        void commit(
                const std::tr1::function<SanitisedDependency (const PackageDepSpec &)> & make_sanitised,
                const std::tr1::function<void (const SanitisedDependency &)> & apply)
        {
            if (seen_qpns.empty())
            {
            }
            else if (1 == seen_qpns.size())
            {
                if (nested)
                    throw InternalError(PALUDIS_HERE, "not implemented");

                if (seen_unadorned)
                {
                    if (seen_slots || seen_additional_requirements || seen_version_requirements || seen_unknown)
                        throw InternalError(PALUDIS_HERE, "not implemented");

                    apply(make_sanitised(make_package_dep_spec(PartiallyMadePackageDepSpecOptions()).package(*seen_qpns.begin())));
                }
                else if (seen_slots)
                {
                    if (seen_additional_requirements || seen_version_requirements || seen_unknown)
                        throw InternalError(PALUDIS_HERE, "not implemented");

                    /* we've got a choice of slots. if any of those slots is unrestricted, pick that. otherwise,
                     * pick the best score, left to right. */
                    bool done(false);
                    std::list<std::list<PackageDepSpec> >::const_iterator g_best(child_groups.end());
                    int best_score(-1);

                    for (std::list<std::list<PackageDepSpec> >::const_iterator g(child_groups.begin()), g_end(child_groups.end()) ;
                            g != g_end ; ++g)
                    {
                        if ((g->size() != 1) || (! g->begin()->slot_requirement_ptr()))
                            throw InternalError(PALUDIS_HERE, "why did that happen?");

                        IsUnrestrictedSlot u;
                        if (g->begin()->slot_requirement_ptr()->accept_returning<bool>(u))
                        {
                            apply(make_sanitised(*g->begin()));
                            done = true;
                            break;
                        }
                        else
                        {
                            int score(resolver.find_any_score(our_qpn_s, make_sanitised(*g->begin())));
                            if (score > best_score)
                            {
                                best_score = score;
                                g_best = g;
                            }
                        }
                    }

                    if (! done)
                    {
                        if (g_best == child_groups.end())
                            throw InternalError(PALUDIS_HERE, "why did that happen?");
                        apply(make_sanitised(*g_best->begin()));
                    }
                }
                else if (seen_additional_requirements || seen_version_requirements)
                {
                    if (seen_additional_requirements && seen_version_requirements)
                        throw InternalError(PALUDIS_HERE, "not implemented");
                    if (seen_slots || seen_unknown)
                        throw InternalError(PALUDIS_HERE, "not implemented");

                    /* we've got a choice of additional requirements or version
                     * requirements. pick the best score, left to right. */
                    std::list<std::list<PackageDepSpec> >::const_iterator g_best(child_groups.end());
                    int best_score(-1);

                    for (std::list<std::list<PackageDepSpec> >::const_iterator g(child_groups.begin()), g_end(child_groups.end()) ;
                            g != g_end ; ++g)
                    {
                        if (g->size() != 1)
                            throw InternalError(PALUDIS_HERE, "why did that happen?");

                        int score(resolver.find_any_score(our_qpn_s, make_sanitised(*g->begin())));
                        if (score > best_score)
                        {
                            best_score = score;
                            g_best = g;
                        }
                    }

                    if (g_best == child_groups.end())
                        throw InternalError(PALUDIS_HERE, "why did that happen?");
                    apply(make_sanitised(*g_best->begin()));
                }
                else if (seen_unknown)
                    throw InternalError(PALUDIS_HERE, "not implemented");
                else
                    throw InternalError(PALUDIS_HERE, "why did that happen?");
            }
            else if (super_complicated)
                throw InternalError(PALUDIS_HERE, "can't");
            else
            {
                /* we've got a choice of groups of packages. pick the best score, left to right. */
                std::list<std::list<PackageDepSpec> >::const_iterator g_best(child_groups.end());
                int best_score(-1);

                for (std::list<std::list<PackageDepSpec> >::const_iterator g(child_groups.begin()), g_end(child_groups.end()) ;
                        g != g_end ; ++g)
                {
                    int worst_score(-1);

                    if (g->empty())
                        throw InternalError(PALUDIS_HERE, "why did that happen?");

                    /* score of a group is the score of the worst child. */
                    for (std::list<PackageDepSpec>::const_iterator h(g->begin()), h_end(g->end()) ;
                            h != h_end ; ++h)
                    {
                        int score(resolver.find_any_score(our_qpn_s, make_sanitised(*h)));
                        if ((-1 == worst_score) || (score < worst_score))
                            worst_score = score;
                    }

                    if ((best_score == -1) || (worst_score > best_score))
                    {
                        best_score = worst_score;
                        g_best = g;
                    }
                }

                if (g_best == child_groups.end())
                    throw InternalError(PALUDIS_HERE, "why did that happen?");
                for (std::list<PackageDepSpec>::const_iterator h(g_best->begin()), h_end(g_best->end()) ;
                        h != h_end ; ++h)
                    apply(make_sanitised(*h));
            }
        }
    };

    struct Finder
    {
        const Resolver & resolver;
        const QPN_S our_qpn_s;
        SanitisedDependencies & sanitised_dependencies;
        std::list<std::tr1::shared_ptr<ActiveDependencyLabels> > labels_stack;

        Finder(
                const Resolver & r,
                const QPN_S & q,
                SanitisedDependencies & s,
                const std::tr1::shared_ptr<const DependencyLabelSequence> & l) :
            resolver(r),
            our_qpn_s(q),
            sanitised_dependencies(s)
        {
            labels_stack.push_front(make_shared_ptr(new ActiveDependencyLabels(*l)));
        }


        void add(const SanitisedDependency & dep)
        {
            sanitised_dependencies.add(dep);
        }

        SanitisedDependency make_sanitised(const PackageDepSpec & spec)
        {
            return make_named_values<SanitisedDependency>(
                    value_for<n::active_dependency_labels>(*labels_stack.begin()),
                    value_for<n::spec>(spec)
                    );
        }

        void visit(const DependencySpecTree::NodeType<PackageDepSpec>::Type & node)
        {
            add(make_sanitised(*node.spec()));
        }

        void visit(const DependencySpecTree::NodeType<BlockDepSpec>::Type &)
        {
        }

        void visit(const DependencySpecTree::NodeType<ConditionalDepSpec>::Type & node)
        {
            if (node.spec()->condition_met())
            {
                labels_stack.push_front(make_shared_ptr(new ActiveDependencyLabels(**labels_stack.begin())));
                std::for_each(indirect_iterator(node.begin()), indirect_iterator(node.end()), accept_visitor(*this));
                labels_stack.pop_front();
            }
        }

        void visit(const DependencySpecTree::NodeType<AllDepSpec>::Type & node)
        {
            labels_stack.push_front(make_shared_ptr(new ActiveDependencyLabels(**labels_stack.begin())));
            std::for_each(indirect_iterator(node.begin()), indirect_iterator(node.end()), accept_visitor(*this));
            labels_stack.pop_front();
        }

        void visit(const DependencySpecTree::NodeType<AnyDepSpec>::Type & node)
        {
            AnyDepSpecChildHandler h(resolver, our_qpn_s, std::tr1::bind(&Finder::make_sanitised, this, std::tr1::placeholders::_1));
            std::for_each(indirect_iterator(node.begin()), indirect_iterator(node.end()), accept_visitor(h));
            h.commit(
                    std::tr1::bind(&Finder::make_sanitised, this, std::tr1::placeholders::_1),
                    std::tr1::bind(&Finder::add, this, std::tr1::placeholders::_1)
                    );
        }

        void visit(const DependencySpecTree::NodeType<NamedSetDepSpec>::Type &) PALUDIS_ATTRIBUTE((noreturn))
        {
            throw InternalError(PALUDIS_HERE, "not implemented");
        }

        void visit(const DependencySpecTree::NodeType<DependencyLabelsDepSpec>::Type & node)
        {
            labels_stack.begin()->reset(new ActiveDependencyLabels(**labels_stack.begin(), *node.spec()));
        }
    };
}

namespace paludis
{
    template <>
    struct Implementation<SanitisedDependencies>
    {
        std::list<SanitisedDependency> sanitised_dependencies;
    };
}

SanitisedDependencies::SanitisedDependencies() :
    PrivateImplementationPattern<SanitisedDependencies>(new Implementation<SanitisedDependencies>)
{
}

SanitisedDependencies::~SanitisedDependencies()
{
}

void
SanitisedDependencies::_populate_one(
        const Resolver & resolver,
        const std::tr1::shared_ptr<const PackageID> & id,
        const std::tr1::shared_ptr<const MetadataSpecTreeKey<DependencySpecTree> > (PackageID::* const pmf) () const
        )
{
    Context context("When finding dependencies for '" + stringify(*id) + "' from key '" + ((*id).*pmf)()->raw_name() + "':");

    Finder f(resolver, QPN_S(id), *this, ((*id).*pmf)()->initial_labels());
    ((*id).*pmf)()->value()->root()->accept(f);
}

void
SanitisedDependencies::populate(
        const Resolver & resolver,
        const std::tr1::shared_ptr<const PackageID> & id)
{
    Context context("When finding dependencies for '" + stringify(*id) + "':");

    if (id->build_dependencies_key())
        _populate_one(resolver, id, &PackageID::build_dependencies_key);
    if (id->run_dependencies_key())
        _populate_one(resolver, id, &PackageID::run_dependencies_key);
    if (id->post_dependencies_key())
        _populate_one(resolver, id, &PackageID::post_dependencies_key);
}

void
SanitisedDependencies::add(const SanitisedDependency & dep)
{
    _imp->sanitised_dependencies.push_back(dep);
}

SanitisedDependencies::ConstIterator
SanitisedDependencies::begin() const
{
    return ConstIterator(_imp->sanitised_dependencies.begin());
}

SanitisedDependencies::ConstIterator
SanitisedDependencies::end() const
{
    return ConstIterator(_imp->sanitised_dependencies.end());
}

std::ostream &
paludis::resolver::operator<< (std::ostream & s, const SanitisedDependency & d)
{
    std::stringstream ss;
    ss << "Dep(" << d.spec();

    if (! d.active_dependency_labels()->system_labels()->empty())
        ss << " system { " << join(indirect_iterator(d.active_dependency_labels()->system_labels()->begin()),
                indirect_iterator(d.active_dependency_labels()->system_labels()->end()), ", ") << " }";
    if (! d.active_dependency_labels()->type_labels()->empty())
        ss << " type { " << join(indirect_iterator(d.active_dependency_labels()->type_labels()->begin()),
                indirect_iterator(d.active_dependency_labels()->type_labels()->end()), ", ") << " }";
    if (! d.active_dependency_labels()->abi_labels()->empty())
        ss << " abi { " << join(indirect_iterator(d.active_dependency_labels()->abi_labels()->begin()),
                indirect_iterator(d.active_dependency_labels()->abi_labels()->end()), ", ") << " }";
    if (! d.active_dependency_labels()->suggest_labels()->empty())
        ss << " suggest { " << join(indirect_iterator(d.active_dependency_labels()->suggest_labels()->begin()),
                indirect_iterator(d.active_dependency_labels()->suggest_labels()->end()), ", ") << " }";

    ss << ")";

    s << ss.str();
    return s;
}

