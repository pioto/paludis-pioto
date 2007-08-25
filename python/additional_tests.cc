/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2007 Piotr Jaroszyński <peper@gentoo.org>
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

#include "additional_tests.hh"

#include <python/paludis_python.hh>

#include <paludis/util/tr1_memory.hh>
#include <paludis/util/set.hh>
#include <paludis/environment.hh>
#include <paludis/package_database.hh>
#include <paludis/repositories/fake/fake_repository.hh>
#include <paludis/repositories/fake/fake_package_id.hh>
#include <paludis/mask.hh>
#include <paludis/hook.hh>

using namespace paludis;
namespace bp = boost::python;

namespace environment
{
    void test_env(Environment & e)
    {
        tr1::shared_ptr<FakeRepository> repo(new FakeRepository(&e, RepositoryName("fakerepo")));
        tr1::shared_ptr<PackageID> pid(repo->add_version("cat", "pkg", "1.0"));
        e.package_database()->add_repository(0, repo);

        UseFlagName u("use");
        e.query_use(u, *pid);

        e.known_use_expand_names(u, *pid);

        e.accept_license("l", *pid);

        tr1::shared_ptr<KeywordNameSet> kns(new KeywordNameSet);
        kns->insert(KeywordName("keyword"));
        e.accept_keywords(kns, *pid);

        e.mask_for_breakage(*pid);

        e.mask_for_user(*pid);

        e.unmasked_by_user(*pid);

        e.package_database();

        e.bashrc_files();

        e.syncers_dirs();

        e.fetchers_dirs();

        e.hook_dirs();

        e.paludis_command();

        e.set_paludis_command("paludis");

        e.root();

        e.reduced_uid();

        e.reduced_gid();

        e.mirrors("mirror");

        e.set_names();

        e.set(SetName("set"));

        e.default_destinations();

        e.perform_hook(Hook("test"));

        e.default_distribution();
    }
}

namespace mask
{
    void test_mask(Mask & m)
    {
        m.key();
        m.description();
    }

    void test_user_mask(UserMask & m)
    {
        test_mask(m);
    }

    void test_unaccepted_mask(UnacceptedMask & m)
    {
        test_mask(m);
        m.unaccepted_key();
    }

    void test_repository_mask(RepositoryMask & m)
    {
        test_mask(m);
        m.mask_key();
    }

    void test_unsupported_mask(UnsupportedMask & m)
    {
        test_mask(m);
        m.explanation();
    }

    void test_association_mask(AssociationMask & m)
    {
        test_mask(m);
        m.associated_package();
    }
}

namespace metadata_key
{
    void test_metadata_key(MetadataKey & m)
    {
        m.raw_name();
        m.human_name();
    }

    void test_metadata_package_id_key(MetadataPackageIDKey & m)
    {
        test_metadata_key(m);
        m.value();
    }

    void test_metadata_string_key(MetadataStringKey & m)
    {
        test_metadata_key(m);
        m.value();
    }

    void test_metadata_time_key(MetadataTimeKey & m)
    {
        test_metadata_key(m);
        m.value();
    }

    void test_metadata_contents_key(MetadataContentsKey & m)
    {
        test_metadata_key(m);
        m.value();
    }

    void test_metadata_repository_mask_info_key(MetadataRepositoryMaskInfoKey & m)
    {
        test_metadata_key(m);
        m.value();
    }

    template <typename C_>
    void test_metadata_set_key(MetadataSetKey<C_> & m)
    {
        test_metadata_key(m);
        m.value();
    }

    template <typename C_>
    void test_metadata_spec_tree_key(MetadataSpecTreeKey<C_> & m)
    {
        test_metadata_key(m);
        m.value();
        m.pretty_print();
        m.pretty_print_flat();
    }
}


void expose_additional_tests()
{
    /**
     * Environemnt tests
     */
    bp::def("test_env", &environment::test_env);

    /**
     * Mask tests
     */
    bp::def("test_user_mask", &mask::test_user_mask);
    bp::def("test_unaccepted_mask", &mask::test_unaccepted_mask);
    bp::def("test_repository_mask", &mask::test_repository_mask);
    bp::def("test_unsupported_mask", &mask::test_unsupported_mask);
    bp::def("test_association_mask", &mask::test_association_mask);

    /**
     * MetadataKey tests
     */
    bp::def("test_metadata_package_id_key", &metadata_key::test_metadata_package_id_key);
    bp::def("test_metadata_string_key", &metadata_key::test_metadata_string_key);
    bp::def("test_metadata_time_key", &metadata_key::test_metadata_time_key);
    bp::def("test_metadata_contents_key", &metadata_key::test_metadata_contents_key);
    bp::def("test_metadata_repository_mask_info_key", &metadata_key::test_metadata_repository_mask_info_key);
    bp::def("test_metadata_keyword_name_set_key", &metadata_key::test_metadata_set_key<KeywordNameSet>);
    bp::def("test_metadata_use_flag_name_set_key", &metadata_key::test_metadata_set_key<UseFlagNameSet>);
    bp::def("test_metadata_iuse_flag_set_key", &metadata_key::test_metadata_set_key<IUseFlagSet>);
    bp::def("test_metadata_inherited_set_key", &metadata_key::test_metadata_set_key<InheritedSet>);
    bp::def("test_metadata_license_spec_tree_key", &metadata_key::test_metadata_spec_tree_key<LicenseSpecTree>);
    bp::def("test_metadata_provide_spec_tree_key", &metadata_key::test_metadata_spec_tree_key<ProvideSpecTree>);
    bp::def("test_metadata_dependency_spec_tree_key", &metadata_key::test_metadata_spec_tree_key<DependencySpecTree>);
    bp::def("test_metadata_restrict_spec_tree_key", &metadata_key::test_metadata_spec_tree_key<RestrictSpecTree>);
    bp::def("test_metadata_uri_spec_tree_key", &metadata_key::test_metadata_spec_tree_key<URISpecTree>);
}