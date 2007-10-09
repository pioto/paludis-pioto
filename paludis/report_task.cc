/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2006 Fernando J. Pereda <ferdy@gentoo.org>
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

#include "report_task.hh"
#include <paludis/util/log.hh>
#include <paludis/uninstall_list.hh>
#include <paludis/environment.hh>
#include <paludis/query.hh>
#include <paludis/metadata_key.hh>
#include <paludis/dep_tag.hh>
#include <paludis/package_id.hh>
#include <paludis/util/private_implementation_pattern-impl.hh>
#include <paludis/util/visitor-impl.hh>
#include <paludis/util/set.hh>
#include <paludis/package_database.hh>
#include <libwrapiter/libwrapiter_forward_iterator.hh>
#include <libwrapiter/libwrapiter_output_iterator.hh>
#include <set>
#include <map>

using namespace paludis;

namespace
{
    class VulnerableChecker :
        public ConstVisitor<SetSpecTree>,
        public ConstVisitor<SetSpecTree>::VisitConstSequence<VulnerableChecker, AllDepSpec>
    {
        private:
            std::multimap<tr1::shared_ptr<const PackageID>, tr1::shared_ptr<const DepTag>, PackageIDSetComparator> _found;
            const Environment & _env;
            std::set<SetName> _recursing_sets;

        public:
            typedef std::multimap<tr1::shared_ptr<const PackageID>, tr1::shared_ptr<const DepTag>,
                    PackageIDSetComparator>::const_iterator ConstIterator;

            using ConstVisitor<SetSpecTree>::VisitConstSequence<VulnerableChecker, AllDepSpec>::visit;

            VulnerableChecker(const Environment & e) :
                _env(e)
            {
            }

            void visit_leaf(const PackageDepSpec &);

            void visit_leaf(const NamedSetDepSpec & s)
            {
                Context context("When expanding named set '" + stringify(s) + "':");

                tr1::shared_ptr<const SetSpecTree::ConstItem> set(_env.set(s.name()));
                if (! set)
                {
                    Log::get_instance()->message(ll_warning, lc_context) << "Unknown set '" << s.name() << "'";
                    return;
                }

                if (! _recursing_sets.insert(s.name()).second)
                {
                    Log::get_instance()->message(ll_warning, lc_context) << "Recursively defined set '" << s.name() << "'";
                    return;
                }

                set->accept(*this);

                _recursing_sets.erase(s.name());
            }

            std::pair<ConstIterator, ConstIterator> insecure_tags(const tr1::shared_ptr<const PackageID> & id) const
            {
                return _found.equal_range(id);
            }
    };

    void
    VulnerableChecker::visit_leaf(const PackageDepSpec & a)
    {
        tr1::shared_ptr<const PackageIDSequence> insecure(
                _env.package_database()->query(query::Matches(a), qo_order_by_version));
        for (PackageIDSequence::ConstIterator i(insecure->begin()),
                i_end(insecure->end()) ; i != i_end ; ++i)
            if (a.tag())
                _found.insert(std::make_pair(*i, a.tag()));
            else
                throw InternalError(PALUDIS_HERE, "didn't get a tag");
    }
}

namespace paludis
{
    template<>
    struct Implementation<ReportTask>
    {
        Environment * const env;

        Implementation(Environment * const e) :
            env(e)
        {
        }
    };
}

ReportTask::ReportTask(Environment * const env) :
    PrivateImplementationPattern<ReportTask>(new Implementation<ReportTask>(env))
{
}

ReportTask::~ReportTask()
{
}

void
ReportTask::execute()
{
    Context context("When executing report task:");

    on_report_all_pre();

    paludis::Environment * const e(_imp->env);
    bool once(true);

    VulnerableChecker vuln(*e);
    for (PackageDatabase::RepositoryConstIterator r(e->package_database()->begin_repositories()),
            r_end(e->package_database()->end_repositories()) ; r != r_end ; ++r)
    {
        tr1::shared_ptr<const Repository> rr(e->package_database()->fetch_repository((*r)->name()));
        if (! rr->sets_interface)
            continue;

        try
        {
            tr1::shared_ptr<const SetSpecTree::ConstItem> insecure(rr->sets_interface->package_set(SetName("insecurity")));
            if (! insecure)
                continue;
            insecure->accept(vuln);
        }
        catch (const NotAvailableError &)
        {
            if (once)
            {
                Log::get_instance()->message(ll_warning, lc_no_context,
                        "Skipping GLSA checks because Paludis was built without XML support");
                once = false;
            }
        }
    }

    UninstallList unused_list(e, UninstallListOptions());
    unused_list.add_unused();
    std::set<tr1::shared_ptr<const PackageID>, PackageIDSetComparator> unused;
    for (UninstallList::ConstIterator i(unused_list.begin()), i_end(unused_list.end());
            i != i_end ; ++i)
        if (i->kind != ulk_virtual)
            unused.insert(i->package_id);

    for (PackageDatabase::RepositoryConstIterator r(e->package_database()->begin_repositories()),
            r_end(e->package_database()->end_repositories()) ; r != r_end ; ++r)
    {
        tr1::shared_ptr<const Repository> rr(e->package_database()->fetch_repository((*r)->name()));
        if (! rr->installed_interface)
            continue;

        tr1::shared_ptr<const CategoryNamePartSet> cat_names(rr->category_names());
        for (CategoryNamePartSet::ConstIterator c(cat_names->begin()), c_end(cat_names->end()) ;
                    c != c_end ; ++c)
        {
            tr1::shared_ptr<const QualifiedPackageNameSet> packages(rr->package_names(*c));
            for (QualifiedPackageNameSet::ConstIterator p(packages->begin()), p_end(packages->end()) ;
                    p != p_end ; ++p)
            {
                on_report_check_package_pre(*p);

                tr1::shared_ptr<const PackageIDSequence> ids(rr->package_ids(*p));
                for (PackageIDSequence::ConstIterator v(ids->begin()), v_end(ids->end()) ;
                        v != v_end ; ++v)
                {
                    bool is_missing(false);
                    tr1::shared_ptr<const PackageID> origin;
                    tr1::shared_ptr<RepositoryName> repo_name;

                    if ((*v)->source_origin_key())
                    {
                        tr1::shared_ptr<QualifiedPackageName> pkg_name(new QualifiedPackageName((*v)->name()));
                        tr1::shared_ptr<VersionRequirements> ver_reqs(make_equal_to_version_requirements((*v)->version()));
                        repo_name.reset(new RepositoryName((*v)->source_origin_key()->value()));

                        tr1::shared_ptr<const PackageIDSequence> installable(
                            e->package_database()->query(
                                query::Matches(
                                    PackageDepSpec(
                                        pkg_name,
                                        tr1::shared_ptr<CategoryNamePart>(),
                                        tr1::shared_ptr<PackageNamePart>(),
                                        ver_reqs,
                                        vr_and,
                                        tr1::shared_ptr<SlotName>(),
                                        repo_name)) &
                                query::SupportsAction<InstallAction>(),
                                qo_best_version_only));

                        if (installable->empty())
                            is_missing = true;
                        else
                            origin = *installable->last();
                    }

                    bool is_masked(origin && origin->masked());
                    bool is_vulnerable(false);
                    bool is_unused(false);

                    std::pair<VulnerableChecker::ConstIterator, VulnerableChecker::ConstIterator> pi(vuln.insecure_tags(*v));
                    if (pi.first != pi.second)
                        is_vulnerable = true;

                    if (unused.end() != unused.find(*v))
                        is_unused = true;

                    if (is_masked || is_vulnerable || is_missing || is_unused)
                    {
                        on_report_package_failure_pre(*v);
                        if (is_masked)
                            on_report_package_is_masked(*v, origin);
                        if (is_vulnerable)
                        {
                            on_report_package_is_vulnerable_pre(*v);
                            for (VulnerableChecker::ConstIterator itag(pi.first) ; itag != pi.second ; ++itag)
                                on_report_package_is_vulnerable(*v, itag->second->short_text());
                            on_report_package_is_vulnerable_post(*v);
                        }
                        if (is_missing)
                            on_report_package_is_missing(*v, *repo_name);
                        if (is_unused)
                            on_report_package_is_unused(*v);
                        on_report_package_failure_post(*v);
                    }
                    else
                        on_report_package_success(*v);
                }

                on_report_check_package_post(*p);
            }
        }
    }

    on_report_all_post();
}
