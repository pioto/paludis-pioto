/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2007 Ciaran McCreesh <ciaranm@ciaranm.org>
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

#include <paludis/repositories/e/eapi.hh>
#include <paludis/name.hh>
#include <paludis/dep_spec.hh>
#include <paludis/hashed_containers.hh>
#include <paludis/util/dir_iterator.hh>
#include <paludis/util/is_file_with_extension.hh>
#include <paludis/util/system.hh>
#include <paludis/util/strip.hh>
#include <paludis/util/destringify.hh>
#include <paludis/util/make_shared_ptr.hh>
#include <paludis/util/tokeniser.hh>
#include <paludis/util/stringify.hh>
#include <paludis/util/private_implementation_pattern-impl.hh>
#include <paludis/util/instantiation_policy-impl.hh>
#include <paludis/config_file.hh>

#include <map>
#include <vector>

using namespace paludis;
using namespace paludis::erepository;

#include <paludis/repositories/e/eapi-sr.cc>

template class InstantiationPolicy<EAPIData, instantiation_method::SingletonTag>;

namespace paludis
{
    template<>
    struct Implementation<EAPIData>
    {
        MakeHashedMap<std::string, tr1::shared_ptr<const EAPI> >::Type values;

        Implementation()
        {
            Context c("When loading EAPI data:");

            for (DirIterator d(getenv_with_default("PALUDIS_EAPIS_DIR", DATADIR "/paludis/eapis")), d_end ;
                    d != d_end ; ++d)
            {
                if (! is_file_with_extension(*d, ".conf", IsFileWithOptions()))
                    continue;

                Context cc("When loading EAPI file '" + stringify(*d) + "':");

                KeyValueConfigFile k(*d, KeyValueConfigFileOptions());

                values.insert(std::make_pair(strip_trailing_string(d->basename(), ".conf"),
                            make_shared_ptr(new EAPI(strip_trailing_string(d->basename(), ".conf"), make_shared_ptr(new SupportedEAPI(
                                            SupportedEAPI::create()
                                            .package_dep_spec_parse_mode(destringify<PackageDepSpecParseMode>(
                                                    k.get("package_dep_spec_parse_mode")))
                                            .strict_package_dep_spec_parse_mode(destringify<PackageDepSpecParseMode>(
                                                    k.get("strict_package_dep_spec_parse_mode").empty() ?
                                                    k.get("package_dep_spec_parse_mode") :
                                                    k.get("strict_package_dep_spec_parse_mode")))
                                            .dependency_spec_tree_parse_mode(destringify<DependencySpecTreeParseMode>(
                                                    k.get("dependency_spec_tree_parse_mode")))
                                            .iuse_flag_parse_mode(destringify<IUseFlagParseMode>(
                                                    k.get("iuse_flag_parse_mode")))
                                            .strict_iuse_flag_parse_mode(destringify<IUseFlagParseMode>(
                                                    k.get("strict_iuse_flag_parse_mode").empty() ?
                                                    k.get("iuse_flag_parse_mode") :
                                                    k.get("strict_iuse_flag_parse_mode")))
                                            .breaks_portage(destringify<bool>(k.get("breaks_portage")))
                                            .uri_supports_arrow(destringify<bool>(k.get("uri_supports_arrow")))

                                            .ebuild_options(make_shared_ptr(new EAPIEbuildOptions(
                                                        EAPIEbuildOptions::create()
                                                        .want_portage_emulation_vars(destringify<bool>(k.get("want_portage_emulation_vars")))
                                                        .require_use_expand_in_iuse(destringify<bool>(k.get("require_use_expand_in_iuse")))
                                                        .rdepend_defaults_to_depend(destringify<bool>(k.get("rdepend_defaults_to_depend")))
                                                        .non_empty_variables(k.get("non_empty_variables"))
                                                        .directory_variables(k.get("directory_variables"))
                                                        .directory_if_exists_variables(k.get("directory_if_exists_variables"))
                                                        .ebuild_must_not_set_variables(k.get("ebuild_must_not_set_variables"))
                                                        .vdb_from_env_variables(k.get("vdb_from_env_variables"))
                                                        .vdb_from_env_unless_empty_variables(k.get("vdb_from_env_unless_empty_variables"))
                                                        .source_merged_variables(k.get("source_merged_variables"))
                                                        .must_not_change_variables(k.get("must_not_change_variables"))
                                                        .save_variables(k.get("save_variables"))
                                                        .save_base_variables(k.get("save_base_variables"))
                                                        .support_eclasses(destringify<bool>(k.get("support_eclasses")))
                                                        .support_exlibs(destringify<bool>(k.get("support_exlibs")))
                                                        .utility_path_suffixes(k.get("utility_path_suffixes"))
                                                        .ebuild_module_suffixes(k.get("ebuild_module_suffixes"))
                                                        .use_expand_separator(destringify<char>(k.get("use_expand_separator")))
                                                        )))

                                            .ebuild_phases(make_shared_ptr(new EAPIEbuildPhases(
                                                            EAPIEbuildPhases::create()
                                                            .ebuild_install(k.get("ebuild_install"))
                                                            .ebuild_uninstall(k.get("ebuild_uninstall"))
                                                            .ebuild_pretend(k.get("ebuild_pretend"))
                                                            .ebuild_metadata(k.get("ebuild_metadata"))
                                                            .ebuild_nofetch(k.get("ebuild_nofetch"))
                                                            .ebuild_variable(k.get("ebuild_variable"))
                                                            .ebuild_config(k.get("ebuild_config")))))

                                            .ebuild_metadata_variables(make_shared_ptr(new EAPIEbuildMetadataVariables(
                                                            EAPIEbuildMetadataVariables::create()
                                                            .metadata_build_depend(k.get("metadata_build_depend"))
                                                            .metadata_run_depend(k.get("metadata_run_depend"))
                                                            .metadata_slot(k.get("metadata_slot"))
                                                            .metadata_src_uri(k.get("metadata_src_uri"))
                                                            .metadata_restrict(k.get("metadata_restrict"))
                                                            .metadata_homepage(k.get("metadata_homepage"))
                                                            .metadata_license(k.get("metadata_license"))
                                                            .metadata_description(k.get("metadata_description"))
                                                            .metadata_keywords(k.get("metadata_keywords"))
                                                            .metadata_eclass_keywords(k.get("metadata_eclass_keywords"))
                                                            .metadata_inherited(k.get("metadata_inherited"))
                                                            .metadata_iuse(k.get("metadata_iuse"))
                                                            .metadata_pdepend(k.get("metadata_pdepend"))
                                                            .metadata_provide(k.get("metadata_provide"))
                                                            .metadata_eapi(k.get("metadata_eapi"))
                                                            .description_build_depend(k.get("description_build_depend"))
                                                            .description_run_depend(k.get("description_run_depend"))
                                                            .description_slot(k.get("description_slot"))
                                                            .description_src_uri(k.get("description_src_uri"))
                                                            .description_restrict(k.get("description_restrict"))
                                                            .description_homepage(k.get("description_homepage"))
                                                            .description_license(k.get("description_license"))
                                                            .description_description(k.get("description_description"))
                                                            .description_keywords(k.get("description_keywords"))
                                                            .description_eclass_keywords(k.get("description_eclass_keywords"))
                                                            .description_inherited(k.get("description_inherited"))
                                                            .description_iuse(k.get("description_iuse"))
                                                            .description_pdepend(k.get("description_pdepend"))
                                                            .description_provide(k.get("description_provide"))
                                                            .description_eapi(k.get("description_eapi"))
                                                            )))

                                            .ebuild_environment_variables(make_shared_ptr(new EAPIEbuildEnvironmentVariables(
                                                            EAPIEbuildEnvironmentVariables::create()
                                                            .env_use(k.get("env_use"))
                                                            .env_use_expand(k.get("env_use_expand"))
                                                            .env_use_expand_hidden(k.get("env_use_expand_hidden"))
                                                            .env_aa(k.get("env_aa"))
                                                            .env_arch(k.get("env_arch"))
                                                            .env_kv(k.get("env_kv"))
                                                            .env_portdir(k.get("env_portdir"))
                                                            .env_distdir(k.get("env_distdir"))
                                                            .env_accept_keywords(k.get("env_accept_keywords"))
                                                            .description_use(k.get("description_use"))
                                                            )))

                                            .uri_labels(make_shared_ptr(new EAPILabels(k.get("uri_labels"))))

                                            ))))));
            }

            MakeHashedMap<std::string, tr1::shared_ptr<const EAPI> >::Type::const_iterator i(values.find("0"));
            if (i == values.end())
                throw EAPIConfigurationError("No EAPI configuration found for EAPI 0");
            else
                values.insert(std::make_pair("", i->second));
        }
    };
}

EAPIConfigurationError::EAPIConfigurationError(const std::string & s) throw () :
    ConfigurationError("EAPI configuration error: " + s)
{
}

EAPIData::EAPIData() :
    PrivateImplementationPattern<EAPIData>(new Implementation<EAPIData>)
{
}

EAPIData::~EAPIData()
{
}

tr1::shared_ptr<const EAPI>
EAPIData::eapi_from_string(const std::string & s) const
{
    MakeHashedMap<std::string, tr1::shared_ptr<const EAPI> >::Type::const_iterator i(_imp->values.find(s));
    if (i != _imp->values.end())
        return i->second;

    return make_shared_ptr(new EAPI(s, tr1::shared_ptr<SupportedEAPI>()));
}

tr1::shared_ptr<const EAPI>
EAPIData::unknown_eapi() const
{
    return make_shared_ptr(new EAPI("UNKNOWN", tr1::shared_ptr<SupportedEAPI>()));
}

namespace paludis
{
    template <>
    struct Implementation<EAPILabels>
    {
        std::map<std::string, std::string> v;
    };
}

EAPILabels::EAPILabels(const std::string & s) :
    PrivateImplementationPattern<EAPILabels>(new Implementation<EAPILabels>)
{
    std::vector<std::string> tokens;
    Tokeniser<delim_kind::AnyOfTag, delim_mode::DelimiterTag> tok(";");
    tok.tokenise(s, std::back_inserter(tokens));

    for (std::vector<std::string>::const_iterator t(tokens.begin()), t_end(tokens.end()) ;
            t != t_end ; ++t)
    {
        std::vector<std::string> values;
        Tokeniser<delim_kind::AnyOfTag, delim_mode::DelimiterTag> vtok("=");
        vtok.tokenise(*t, std::back_inserter(values));

        if (values.size() != 2)
            throw EAPIConfigurationError("EAPI labels value '" + s + "' has bad values size '" + stringify(values.size()) + "'");

        _imp->v.insert(std::make_pair(strip_leading(strip_trailing(values[0], " \t\r\n"), " \t\r\n"),
                    strip_leading(strip_trailing(values[1], " \t\r\n"), " \t\r\n")));
    }
}

EAPILabels::~EAPILabels()
{
}

const std::string
EAPILabels::class_for_label(const std::string & s) const
{
    std::map<std::string, std::string>::const_iterator i(_imp->v.find(s));
    if (_imp->v.end() == i)
        return "";
    return i->second;
}
