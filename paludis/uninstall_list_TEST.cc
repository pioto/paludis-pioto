/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2006, 2007 Ciaran McCreesh <ciaranm@ciaranm.org>
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

#include <paludis/uninstall_list.hh>
#include <paludis/repositories/fake/fake_repository.hh>
#include <paludis/repositories/fake/fake_installed_repository.hh>
#include <paludis/repositories/fake/fake_package_id.hh>
#include <paludis/repositories/virtuals/virtuals_repository.hh>
#include <paludis/environments/test/test_environment.hh>
#include <paludis/util/visitor-impl.hh>
#include <paludis/package_database.hh>
#include <test/test_framework.hh>
#include <test/test_runner.hh>
#include <libwrapiter/libwrapiter_forward_iterator.hh>
#include <libwrapiter/libwrapiter_output_iterator.hh>
#include <string>
#include <list>
#include <ostream>

using namespace paludis;
using namespace test;

namespace paludis
{
    std::ostream &
    operator<< (std::ostream & s, const UninstallListEntry & e)
    {
        s << *e.package_id;
        return s;
    }
}

namespace test_cases
{
    /**
     * Convenience base class used by many of the UninstallList tests.
     *
     */
    class UninstallListTestCaseBase :
        public TestCase
    {
        protected:
            TestEnvironment env;
            tr1::shared_ptr<FakeInstalledRepository> installed_repo;
            tr1::shared_ptr<VirtualsRepository> virtuals_repo;
            tr1::shared_ptr<PackageIDSequence> targets;
            std::list<std::string> expected;
            bool done_populate;

            /**
             * Constructor.
             */
            UninstallListTestCaseBase(const std::string & s) :
                TestCase("uninstall list " + s),
                env(),
                installed_repo(new FakeInstalledRepository(&env, RepositoryName("installed"))),
                virtuals_repo(new VirtualsRepository(&env)),
                targets(new PackageIDSequence),
                done_populate(false)
            {
                env.package_database()->add_repository(2, installed_repo);
                env.package_database()->add_repository(1, virtuals_repo);
            }

            /**
             * Populate our repo member.
             */
            virtual void populate_repo() = 0;

            /**
             * Populate our targets.
             */
            virtual void populate_targets() = 0;

            void add_target(const std::string & p, const std::string & v)
            {
                targets->push_back(
                        env.fetch_package_id(
                            QualifiedPackageName(p),
                            VersionSpec(v),
                            RepositoryName("installed")));
            }

            /**
             * Populate our expected member.
             */
            virtual void populate_expected() = 0;

            /**
             * Check expected is what we got.
             */
            virtual void check_lists()
            {
                TEST_CHECK(true);
                UninstallList d(&env, options());
                for (PackageIDSequence::ConstIterator i(targets->begin()),
                        i_end(targets->end()) ; i != i_end ; ++i)
                    d.add(*i);
                TEST_CHECK(true);

                unsigned n(0);
                std::list<std::string>::const_iterator exp(expected.begin());
                UninstallList::ConstIterator got(d.begin());
                while (true)
                {
                    TestMessageSuffix s(stringify(n++), true);

                    TEST_CHECK((exp == expected.end()) == (got == d.end()));
                    if (got == d.end())
                        break;
                    TEST_CHECK_STRINGIFY_EQUAL(*got, *exp);
                    ++exp;
                    ++got;
                }
            }

            virtual UninstallListOptions options()
            {
                return UninstallListOptions();
            }

        public:
            void run()
            {
                if (! done_populate)
                {
                    populate_repo();
                    populate_targets();
                    populate_expected();
                    done_populate = true;
                }
                check_lists();
            }
    };

    struct UninstallListSimpleTest :
        public UninstallListTestCaseBase
    {
        UninstallListSimpleTest() : UninstallListTestCaseBase("simple") { }

        void populate_targets()
        {
            add_target("foo/bar", "1");
        }

        void populate_repo()
        {
            installed_repo->add_version("foo", "bar", "1");
        }

        void populate_expected()
        {
            expected.push_back("foo/bar-1:0::installed");
        }
    } uninstall_list_simple_test;

    struct UninstallListRepeatTest :
        public UninstallListTestCaseBase
    {
        UninstallListRepeatTest() : UninstallListTestCaseBase("repeat") { }

        void populate_targets()
        {
            add_target("foo/bar", "1");
            add_target("foo/bar", "1");
        }

        void populate_repo()
        {
            installed_repo->add_version("foo", "bar", "1");
        }

        void populate_expected()
        {
            expected.push_back("foo/bar-1:0::installed");
        }
    } uninstall_list_repeat_test;

    struct UninstallListWithUnusedDepsTest :
        public UninstallListTestCaseBase
    {
        UninstallListWithUnusedDepsTest() : UninstallListTestCaseBase("with unused deps") { }

        void populate_targets()
        {
            add_target("foo/bar", "1");
        }

        void populate_repo()
        {
            installed_repo->add_version("foo", "bar", "1")->build_dependencies_key()->set_from_string("foo/baz");
            installed_repo->add_version("foo", "baz", "2");
        }

        void populate_expected()
        {
            expected.push_back("foo/bar-1:0::installed");
            expected.push_back("foo/baz-2:0::installed");
        }

        UninstallListOptions options()
        {
            return UninstallListOptions::create()
                .with_unused_dependencies(true)
                .with_dependencies_included(false)
                .with_dependencies_as_errors(false);
        }
    } uninstall_list_with_unused_deps_test;

    struct UninstallListWithUnusedDepsRecursiveTest :
        public UninstallListTestCaseBase
    {
        UninstallListWithUnusedDepsRecursiveTest() : UninstallListTestCaseBase("with unused deps recursive") { }

        void populate_targets()
        {
            add_target("foo/bar", "1");
        }

        void populate_repo()
        {
            installed_repo->add_version("foo", "bar", "1")->build_dependencies_key()->set_from_string("foo/baz");
            installed_repo->add_version("foo", "baz", "2")->build_dependencies_key()->set_from_string("foo/moo");
            installed_repo->add_version("foo", "moo", "3");
        }

        void populate_expected()
        {
            expected.push_back("foo/bar-1:0::installed");
            expected.push_back("foo/baz-2:0::installed");
            expected.push_back("foo/moo-3:0::installed");
        }

        UninstallListOptions options()
        {
            return UninstallListOptions::create()
                .with_unused_dependencies(true)
                .with_dependencies_included(false)
                .with_dependencies_as_errors(false);
        }
    } uninstall_list_with_unused_deps_recursive_test;

    struct UninstallListWithUnusedDepsWithUsedTest :
        public UninstallListTestCaseBase
    {
        UninstallListWithUnusedDepsWithUsedTest() : UninstallListTestCaseBase("with unused deps with used") { }

        void populate_targets()
        {
            add_target("foo/bar", "1");
        }

        void populate_repo()
        {
            installed_repo->add_version("foo", "bar", "1")->build_dependencies_key()->set_from_string("foo/baz foo/oink");
            installed_repo->add_version("foo", "baz", "2");
            installed_repo->add_version("foo", "moo", "3")->build_dependencies_key()->set_from_string("foo/oink");
            installed_repo->add_version("foo", "oink", "1");
        }

        void populate_expected()
        {
            expected.push_back("foo/bar-1:0::installed");
            expected.push_back("foo/baz-2:0::installed");
        }

        UninstallListOptions options()
        {
            return UninstallListOptions::create()
                .with_unused_dependencies(true)
                .with_dependencies_included(false)
                .with_dependencies_as_errors(false);
        }
    } uninstall_list_with_unused_deps_with_used_test;

    struct UninstallListWithUnusedDepsWithCrossUsedTest :
        public UninstallListTestCaseBase
    {
        UninstallListWithUnusedDepsWithCrossUsedTest() :
            UninstallListTestCaseBase("with unused deps with cross used") { }

        void populate_targets()
        {
            add_target("foo/moo", "3");
            add_target("foo/bar", "1");
        }

        void populate_repo()
        {
            installed_repo->add_version("foo", "bar", "1")->build_dependencies_key()->set_from_string("foo/baz foo/oink");
            installed_repo->add_version("foo", "baz", "2");
            installed_repo->add_version("foo", "moo", "3")->build_dependencies_key()->set_from_string("foo/oink");
            installed_repo->add_version("foo", "oink", "1");
        }

        void populate_expected()
        {
            expected.push_back("foo/moo-3:0::installed");
            expected.push_back("foo/bar-1:0::installed");
            expected.push_back("foo/baz-2:0::installed");
            expected.push_back("foo/oink-1:0::installed");
        }

        UninstallListOptions options()
        {
            return UninstallListOptions::create()
                .with_unused_dependencies(true)
                .with_dependencies_as_errors(false)
                .with_dependencies_included(false);
        }
    } uninstall_list_with_unused_deps_with_cross_used_test;

    struct UninstallListWithUnusedDepsWorldTest :
        public UninstallListTestCaseBase
    {
        UninstallListWithUnusedDepsWorldTest() :
            UninstallListTestCaseBase("with unused deps world")
        {
            tr1::shared_ptr<ConstTreeSequence<SetSpecTree, AllDepSpec> > world(new ConstTreeSequence<SetSpecTree, AllDepSpec>(
                        tr1::shared_ptr<AllDepSpec>(new AllDepSpec)));
            world->add(tr1::shared_ptr<TreeLeaf<SetSpecTree, PackageDepSpec> >(new TreeLeaf<SetSpecTree, PackageDepSpec>(
                            tr1::shared_ptr<PackageDepSpec>(new PackageDepSpec("foo/moo", pds_pm_permissive)))));
            installed_repo->add_package_set(SetName("world"), world);
        }

        void populate_targets()
        {
            add_target("foo/bar", "1");
        }

        void populate_repo()
        {
            installed_repo->add_version("foo", "bar", "1")->build_dependencies_key()->set_from_string("foo/baz foo/moo");
            installed_repo->add_version("foo", "baz", "2");
            installed_repo->add_version("foo", "moo", "2");
        }

        void populate_expected()
        {
            expected.push_back("foo/bar-1:0::installed");
            expected.push_back("foo/baz-2:0::installed");
        }

        UninstallListOptions options()
        {
            return UninstallListOptions::create()
                .with_unused_dependencies(true)
                .with_dependencies_included(false)
                .with_dependencies_as_errors(false);
        }
    } uninstall_list_with_unused_deps_world_test;

    struct UninstallListWithUnusedDepsWorldTargetTest :
        public UninstallListTestCaseBase
    {
        UninstallListWithUnusedDepsWorldTargetTest() :
            UninstallListTestCaseBase("with unused deps world target")
        {
            tr1::shared_ptr<ConstTreeSequence<SetSpecTree, AllDepSpec> > world(new ConstTreeSequence<SetSpecTree, AllDepSpec>(
                        tr1::shared_ptr<AllDepSpec>(new AllDepSpec)));
            world->add(tr1::shared_ptr<TreeLeaf<SetSpecTree, PackageDepSpec> >(new TreeLeaf<SetSpecTree, PackageDepSpec>(
                            tr1::shared_ptr<PackageDepSpec>(new PackageDepSpec("foo/moo", pds_pm_permissive)))));
            world->add(tr1::shared_ptr<TreeLeaf<SetSpecTree, PackageDepSpec> >(new TreeLeaf<SetSpecTree, PackageDepSpec>(
                            tr1::shared_ptr<PackageDepSpec>(new PackageDepSpec("foo/bar", pds_pm_permissive)))));
            installed_repo->add_package_set(SetName("world"), world);
        }

        void populate_targets()
        {
            add_target("foo/bar", "1");
        }

        void populate_repo()
        {
            installed_repo->add_version("foo", "bar", "1")->build_dependencies_key()->set_from_string("foo/baz foo/moo");
            installed_repo->add_version("foo", "baz", "2");
            installed_repo->add_version("foo", "moo", "2");
        }

        void populate_expected()
        {
            expected.push_back("foo/bar-1:0::installed");
            expected.push_back("foo/baz-2:0::installed");
        }

        UninstallListOptions options()
        {
            return UninstallListOptions::create()
                .with_unused_dependencies(true)
                .with_dependencies_included(false)
                .with_dependencies_as_errors(false);
        }
    } uninstall_list_with_unused_deps_world_target_test;
}

