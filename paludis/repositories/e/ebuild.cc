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

#include <paludis/repositories/e/ebuild.hh>
#include <paludis/repositories/e/ebuild_id.hh>
#include <paludis/repositories/e/e_repository.hh>

#include <paludis/util/system.hh>
#include <paludis/util/strip.hh>
#include <paludis/util/pstream.hh>
#include <paludis/util/log.hh>
#include <paludis/util/collection_concrete.hh>

#include <paludis/about.hh>
#include <paludis/environment.hh>
#include <paludis/config_file.hh>
#include <paludis/eapi.hh>
#include <paludis/package_id.hh>
#include <paludis/metadata_key.hh>
#include <paludis/portage_dep_parser.hh>

#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include <libwrapiter/libwrapiter_forward_iterator.hh>
#include <libwrapiter/libwrapiter_output_iterator.hh>

#include "config.h"

/** \file
 * Implementation for ebuild.hh things.
 *
 * \ingroup grpebuildinterface
 */

using namespace paludis;
using namespace paludis::erepository;

#include <paludis/repositories/e/ebuild-sr.cc>

EbuildCommand::EbuildCommand(const EbuildCommandParams & p) :
    params(p)
{
}

EbuildCommand::~EbuildCommand()
{
}

bool
EbuildCommand::success()
{
    return true;
}

bool
EbuildCommand::failure()
{
    return false;
}

bool
EbuildCommand::operator() ()
{
    Command cmd(getenv_with_default("PALUDIS_EBUILD_DIR", LIBEXECDIR "/paludis") +
            "/ebuild.bash '" + ebuild_file() + "' " + commands());

    if (params.sandbox)
        cmd.with_sandbox();

    if (params.userpriv)
        cmd.with_uid_gid(params.environment->reduced_uid(), params.environment->reduced_gid());


    tr1::shared_ptr<const FSEntryCollection> syncers_dirs(params.environment->syncers_dirs());
    tr1::shared_ptr<const FSEntryCollection> bashrc_files(params.environment->bashrc_files());
    tr1::shared_ptr<const FSEntryCollection> fetchers_dirs(params.environment->fetchers_dirs());
    tr1::shared_ptr<const FSEntryCollection> hook_dirs(params.environment->hook_dirs());

    cmd = extend_command(cmd
            .with_setenv("P", stringify(params.package_id->name().package) + "-" +
                stringify(params.package_id->version().remove_revision()))
            .with_setenv("PV", stringify(params.package_id->version().remove_revision()))
            .with_setenv("PR", stringify(params.package_id->version().revision_only()))
            .with_setenv("PN", stringify(params.package_id->name().package))
            .with_setenv("PVR", stringify(params.package_id->version()))
            .with_setenv("PF", stringify(params.package_id->name().package) + "-" +
                stringify(params.package_id->version()))
            .with_setenv("CATEGORY", stringify(params.package_id->name().category))
            .with_setenv("REPOSITORY", stringify(params.package_id->repository()->name()))
            .with_setenv("FILESDIR", stringify(params.files_dir))
            .with_setenv("PORTDIR", stringify(params.portdir))
            .with_setenv("DISTDIR", stringify(params.distdir))
            .with_setenv("EAPI", stringify(params.package_id->eapi()->name))
            .with_setenv("SLOT", "")
            .with_setenv("PKGMANAGER", PALUDIS_PACKAGE "-" + stringify(PALUDIS_VERSION_MAJOR) + "." +
                stringify(PALUDIS_VERSION_MINOR) + "." +
                stringify(PALUDIS_VERSION_MICRO) +
                (std::string(PALUDIS_SUBVERSION_REVISION).empty() ?
                 std::string("") : "-r" + std::string(PALUDIS_SUBVERSION_REVISION)))
            .with_setenv("PALUDIS_TMPDIR", stringify(params.buildroot))
            .with_setenv("PALUDIS_CONFIG_DIR", SYSCONFDIR "/paludis/")
            .with_setenv("PALUDIS_BASHRC_FILES", join(bashrc_files->begin(), bashrc_files->end(), " "))
            .with_setenv("PALUDIS_HOOK_DIRS", join(hook_dirs->begin(), hook_dirs->end(), " "))
            .with_setenv("PALUDIS_FETCHERS_DIRS", join(fetchers_dirs->begin(), fetchers_dirs->end(), " "))
            .with_setenv("PALUDIS_SYNCERS_DIRS", join(syncers_dirs->begin(), syncers_dirs->end(), " "))
            .with_setenv("PALUDIS_COMMAND", params.environment->paludis_command())
            .with_setenv("PALUDIS_REDUCED_GID", stringify(params.environment->reduced_gid()))
            .with_setenv("PALUDIS_REDUCED_UID", stringify(params.environment->reduced_uid()))
            .with_setenv("PALUDIS_EBUILD_LOG_LEVEL", stringify(Log::get_instance()->log_level()))
            .with_setenv("PALUDIS_EBUILD_DIR", getenv_with_default("PALUDIS_EBUILD_DIR", LIBEXECDIR "/paludis"))
            .with_setenv("PALUDIS_UTILITY_PATH_SUFFIXES",
                    params.package_id->eapi()->supported->ebuild_options->utility_path_suffixes)
            .with_setenv("PALUDIS_EBUILD_MODULE_SUFFIXES",
                    params.package_id->eapi()->supported->ebuild_options->ebuild_module_suffixes)
            .with_setenv("PALUDIS_NON_EMPTY_VARIABLES",
                    params.package_id->eapi()->supported->ebuild_options->non_empty_variables)
            .with_setenv("PALUDIS_DIRECTORY_VARIABLES",
                    params.package_id->eapi()->supported->ebuild_options->directory_variables)
            .with_setenv("PALUDIS_EBUILD_MUST_NOT_SET_VARIABLES",
                    params.package_id->eapi()->supported->ebuild_options->ebuild_must_not_set_variables)
            .with_setenv("PALUDIS_DIRECTORY_IF_EXISTS_VARIABLES",
                    params.package_id->eapi()->supported->ebuild_options->directory_if_exists_variables)
            .with_setenv("PALUDIS_SOURCE_MERGED_VARIABLES",
                    params.package_id->eapi()->supported->ebuild_options->source_merged_variables)
            .with_setenv("PALUDIS_MUST_NOT_CHANGE_VARIABLES",
                    params.package_id->eapi()->supported->ebuild_options->must_not_change_variables)
            .with_setenv("PALUDIS_RDEPEND_DEFAULTS_TO_DEPEND",
                    params.package_id->eapi()->supported->ebuild_options->rdepend_defaults_to_depend ? "yes" : "")
            );

    if (params.package_id->eapi()->supported->ebuild_options->want_kv_var)
        cmd.with_setenv("KV", kernel_version());

    if (params.package_id->eapi()->supported->ebuild_options->support_eclasses)
        cmd
            .with_setenv("ECLASSDIR", stringify(*params.eclassdirs->begin()))
            .with_setenv("ECLASSDIRS", join(params.eclassdirs->begin(),
                        params.eclassdirs->end(), " "));

    if (params.package_id->eapi()->supported->ebuild_options->support_exlibs)
        cmd
            .with_setenv("EXLIBSDIRS", join(params.exlibsdirs->begin(),
                        params.exlibsdirs->end(), " "));

    if (params.package_id->eapi()->supported->ebuild_options->want_portage_emulation_vars)
        cmd = add_portage_vars(cmd);

    if (do_run_command(cmd))
        return success();
    else
        return failure();
}

std::string
EbuildCommand::ebuild_file() const
{
    return stringify(params.ebuild_file);
}

Command
EbuildCommand::add_portage_vars(const Command & cmd) const
{
    return Command(cmd)
        .with_setenv("PORTAGE_ACTUAL_DISTDIR", stringify(params.distdir))
        .with_setenv("PORTAGE_BASHRC", "/dev/null")
        .with_setenv("PORTAGE_BUILDDIR", stringify(params.buildroot) + "/" +
             stringify(params.package_id->name().category) + "/" +
             stringify(params.package_id->name().package) + "-" +
             stringify(params.package_id->version()))
        .with_setenv("PORTAGE_CALLER", params.environment->paludis_command())
        .with_setenv("PORTAGE_GID", "0")
        .with_setenv("PORTAGE_INST_GID", "0")
        .with_setenv("PORTAGE_INST_UID", "0")
        .with_setenv("PORTAGE_MASTER_PID", stringify(::getpid()))
        .with_setenv("PORTAGE_NICENCESS", stringify(::getpriority(PRIO_PROCESS, 0)))
        .with_setenv("PORTAGE_TMPDIR", stringify(params.buildroot))
        .with_setenv("PORTAGE_TMPFS", "/dev/shm")
        .with_setenv("PORTAGE_WORKDIR_MODE", "0700");
}

bool
EbuildCommand::do_run_command(const Command & cmd)
{
    return 0 == run_command(cmd);
}

EbuildMetadataCommand::EbuildMetadataCommand(const EbuildCommandParams & p) :
    EbuildCommand(p)
{
}

EbuildMetadataCommand::~EbuildMetadataCommand()
{
}

std::string
EbuildMetadataCommand::commands() const
{
    return params.commands;
}

bool
EbuildMetadataCommand::failure()
{
    return EbuildCommand::failure();
}

Command
EbuildMetadataCommand::extend_command(const Command & cmd)
{
    return Command(cmd)
        .with_uid_gid(params.environment->reduced_uid(), params.environment->reduced_gid())
        .with_stderr_prefix(stringify(params.package_id->name().package) + "-" +
                stringify(params.package_id->version()) + "> ");
}

bool
EbuildMetadataCommand::do_run_command(const Command & cmd)
{
    bool ok(false);
    keys.reset(new AssociativeCollection<std::string, std::string>::Concrete);

    try
    {
        Context context("When generating metadata for '" + stringify(*params.package_id) + "':");

        PStream prog(cmd);
        KeyValueConfigFile f(prog, KeyValueConfigFileOptions() + kvcfo_disallow_continuations + kvcfo_disallow_comments
                + kvcfo_disallow_space_around_equals + kvcfo_disallow_unquoted_values + kvcfo_disallow_source
                + kvcfo_disallow_variables + kvcfo_preserve_whitespace);

        std::copy(f.begin(), f.end(), keys->inserter());
        if (0 == prog.exit_status())
            ok = true;
    }
    catch (const NameError & e)
    {
        Log::get_instance()->message(ll_warning, lc_context, "Caught exception '" +
                stringify(e.message()) + "' (" + stringify(e.what()) +
                ") when generating cache for '" + stringify(*params.package_id) + "'");
    }

    if (ok)
        return true;
    else
    {
        Log::get_instance()->message(ll_warning, lc_context) << "Could not generate cache for '"
            << *params.package_id << "'";
        keys.reset(new AssociativeCollection<std::string, std::string>::Concrete);
        keys->insert("EAPI", EAPIData::get_instance()->unknown_eapi()->name);
        keys->insert("SLOT", "UNKNOWN");

        return false;
    }
}

namespace
{
    std::string get(const tr1::shared_ptr<const AssociativeCollection<std::string, std::string> > & k, const std::string & s)
    {
        AssociativeCollection<std::string, std::string>::Iterator i(k->find(s));
        if (k->end() == i)
            return "";
        return i->second;
    }
}

void
EbuildMetadataCommand::load(const tr1::shared_ptr<const EbuildID> & id)
{
    Context context("When loading generated metadata for '" + stringify(*params.package_id) + "':");

    if (! keys)
        throw InternalError(PALUDIS_HERE, "keys is 0");

    if (! id->eapi()->supported)
    {
        Log::get_instance()->message(ll_debug, lc_context) << "ID pre-load EAPI '" << id->eapi()->name << "' not supported";
        return;
    }
    else
        Log::get_instance()->message(ll_debug, lc_context) << "ID pre-load EAPI '" << id->eapi()->name << "' is supported";

    std::string s;
    if (! ((s = get(keys, id->eapi()->supported->ebuild_metadata_variables->metadata_eapi))).empty())
        id->set_eapi(s);
    else
        id->set_eapi(id->e_repository()->params().eapi_when_unspecified);

    if (! id->eapi()->supported)
    {
        Log::get_instance()->message(ll_debug, lc_context) << "ID post-load EAPI '" << id->eapi()->name << "' not supported";
        return;
    }
    else
        Log::get_instance()->message(ll_debug, lc_context) << "ID post-load EAPI '" << id->eapi()->name << "' is supported";

    const EAPIEbuildMetadataVariables & m(*id->eapi()->supported->ebuild_metadata_variables);

    if (! m.metadata_description.empty())
        id->load_short_description(m.metadata_description, "Description", get(keys, m.metadata_description));

    if (! m.metadata_build_depend.empty())
        id->load_build_depend(m.metadata_build_depend, "Build depend", get(keys, m.metadata_build_depend));

    if (! m.metadata_run_depend.empty())
        id->load_run_depend(m.metadata_run_depend, "Run depend", get(keys, m.metadata_run_depend));

    if (! m.metadata_pdepend.empty())
        id->load_post_depend(m.metadata_pdepend, "Post depend", get(keys, m.metadata_pdepend));

    if (! m.metadata_slot.empty())
    {
        try
        {
            Context c("When setting SLOT:");
            std::string slot(get(keys, m.metadata_slot));
            if (slot.empty())
            {
                Log::get_instance()->message(ll_qa, lc_context) << "Package '" << *id << "' set SLOT=\"\", using SLOT=\"0\" instead";
                slot = "0";
            }
            id->set_slot(SlotName(slot));
        }
        catch (const Exception & e)
        {
            Log::get_instance()->message(ll_warning, lc_context) << "Setting SLOT for '" << *id << "' failed due to exception '"
                << e.message() << "' (" << e.what() << ")";
            id->set_slot(SlotName("0"));
        }
    }

    if (! m.metadata_src_uri.empty())
        id->load_src_uri(m.metadata_src_uri, "Source URI", get(keys, m.metadata_src_uri));

    if (! m.metadata_homepage.empty())
        id->load_homepage(m.metadata_homepage, "Homepage", get(keys, m.metadata_homepage));

    if (! m.metadata_license.empty())
        id->load_license(m.metadata_license, "License", get(keys, m.metadata_license));

    if (! m.metadata_provide.empty())
        id->load_provide(m.metadata_provide, "Provides", get(keys, m.metadata_provide));

    if (! m.metadata_iuse.empty())
        id->load_iuse(m.metadata_iuse, "Used USE flags", get(keys, m.metadata_iuse));

    if (! m.metadata_inherited.empty())
        id->load_inherited(m.metadata_inherited, "Inherited", get(keys, m.metadata_inherited));

    if (! m.metadata_keywords.empty())
        id->load_keywords(m.metadata_keywords, "Keywords", get(keys, m.metadata_keywords));

    if (! m.metadata_restrict.empty())
        id->load_restrict(m.metadata_restrict, "Restrictions", get(keys, m.metadata_restrict));
}

EbuildVariableCommand::EbuildVariableCommand(const EbuildCommandParams & p,
        const std::string & var) :
    EbuildCommand(p),
    _var(var)
{
}

std::string
EbuildVariableCommand::commands() const
{
    return params.commands;
}

bool
EbuildVariableCommand::failure()
{
    return EbuildCommand::failure();
}

Command
EbuildVariableCommand::extend_command(const Command & cmd)
{
    return Command(cmd)
        .with_setenv("PALUDIS_VARIABLE", _var)
        .with_uid_gid(params.environment->reduced_uid(), params.environment->reduced_gid());
}

bool
EbuildVariableCommand::do_run_command(const Command & cmd)
{
    PStream prog(cmd);
    _result = strip_trailing_string(
            std::string((std::istreambuf_iterator<char>(prog)),
                std::istreambuf_iterator<char>()), "\n");

    return (0 == prog.exit_status());
}

std::string
EbuildFetchCommand::commands() const
{
    return params.commands;
}

bool
EbuildFetchCommand::failure()
{
    throw PackageFetchActionError("Fetch failed for '" + stringify(*params.package_id) + "'");
}

Command
EbuildFetchCommand::extend_command(const Command & cmd)
{
    Command result(Command(cmd)
            .with_setenv("A", fetch_params.a)
            .with_setenv("AA", fetch_params.aa)
            .with_setenv("USE", fetch_params.use)
            .with_setenv("USE_EXPAND", fetch_params.use_expand)
            .with_setenv("FLAT_SRC_URI", fetch_params.flat_src_uri)
            .with_setenv("ROOT", fetch_params.root)
            .with_setenv("PALUDIS_USE_SAFE_RESUME", fetch_params.safe_resume ? "oohyesplease" : "")
            .with_setenv("PALUDIS_PROFILE_DIR", stringify(*fetch_params.profiles->begin()))
            .with_setenv("PALUDIS_PROFILE_DIRS", join(fetch_params.profiles->begin(),
                    fetch_params.profiles->end(), " ")));

    for (AssociativeCollection<std::string, std::string>::Iterator
            i(fetch_params.expand_vars->begin()),
            j(fetch_params.expand_vars->end()) ; i != j ; ++i)
        result.with_setenv(i->first, i->second);

    return result;
}

EbuildFetchCommand::EbuildFetchCommand(const EbuildCommandParams & p,
        const EbuildFetchCommandParams & f) :
    EbuildCommand(p),
    fetch_params(f)
{
}

std::string
EbuildInstallCommand::commands() const
{
    return params.commands;
}

bool
EbuildInstallCommand::failure()
{
    throw PackageInstallActionError("Install failed for '" + stringify(*params.package_id) + "'");
}

Command
EbuildInstallCommand::extend_command(const Command & cmd)
{
    std::string debug_build;
    do
    {
        switch (install_params.debug_build)
        {
            case ido_none:
                debug_build = "none";
                continue;

            case ido_split:
                debug_build = "split";
                continue;

            case ido_internal:
                debug_build = "internal";
                continue;
        }

        throw InternalError(PALUDIS_HERE, "Bad debug_build value");
    }
    while (false);

    Command result(Command(cmd)
            .with_setenv("A", install_params.a)
            .with_setenv("AA", install_params.aa)
            .with_setenv("USE", install_params.use)
            .with_setenv("USE_EXPAND", install_params.use_expand)
            .with_setenv("ROOT", install_params.root)
            .with_setenv("PALUDIS_LOADSAVEENV_DIR", stringify(install_params.loadsaveenv_dir))
            .with_setenv("PALUDIS_CONFIG_PROTECT", install_params.config_protect)
            .with_setenv("PALUDIS_CONFIG_PROTECT_MASK", install_params.config_protect_mask)
            .with_setenv("PALUDIS_EBUILD_OVERRIDE_CONFIG_PROTECT_MASK",
                install_params.disable_cfgpro ? "/" : "")
            .with_setenv("PALUDIS_DEBUG_BUILD", debug_build)
            .with_setenv("PALUDIS_PROFILE_DIR", stringify(*install_params.profiles->begin()))
            .with_setenv("PALUDIS_PROFILE_DIRS", join(install_params.profiles->begin(),
                                          install_params.profiles->end(), " "))
            .with_setenv("SLOT", stringify(install_params.slot)));

    for (AssociativeCollection<std::string, std::string>::Iterator
            i(install_params.expand_vars->begin()),
            j(install_params.expand_vars->end()) ; i != j ; ++i)
        result.with_setenv(i->first, i->second);

    return result;
}

EbuildInstallCommand::EbuildInstallCommand(const EbuildCommandParams & p,
        const EbuildInstallCommandParams & f) :
    EbuildCommand(p),
    install_params(f)
{
}

std::string
EbuildUninstallCommand::commands() const
{
    return params.commands;
}

std::string
EbuildUninstallCommand::ebuild_file() const
{
    return "-";
}

bool
EbuildUninstallCommand::failure()
{
    throw PackageUninstallActionError("Uninstall failed for '" + stringify(*params.package_id) + "'");
}

Command
EbuildUninstallCommand::extend_command(const Command & cmd)
{
    Command result(Command(cmd)
            .with_setenv("ROOT", uninstall_params.root)
            .with_setenv("PALUDIS_LOADSAVEENV_DIR", stringify(uninstall_params.loadsaveenv_dir))
            .with_setenv("PALUDIS_EBUILD_OVERRIDE_CONFIG_PROTECT_MASK",
                uninstall_params.disable_cfgpro ? "/" : ""));

    if (uninstall_params.load_environment)
        result
            .with_setenv("PALUDIS_LOAD_ENVIRONMENT", stringify(*uninstall_params.load_environment))
            .with_setenv("PALUDIS_SKIP_INHERIT", "yes");

    return result;
}

EbuildUninstallCommand::EbuildUninstallCommand(const EbuildCommandParams & p,
        const EbuildUninstallCommandParams & f) :
    EbuildCommand(p),
    uninstall_params(f)
{
}

std::string
EbuildConfigCommand::commands() const
{
    return params.commands;
}

bool
EbuildConfigCommand::failure()
{
    throw PackageConfigActionError("Configure failed for '" + stringify(
                *params.package_id) + "'");
}

Command
EbuildConfigCommand::extend_command(const Command & cmd)
{
    Command result(Command(cmd)
            .with_setenv("ROOT", config_params.root));

    if (config_params.load_environment)
        result
            .with_setenv("PALUDIS_LOAD_ENVIRONMENT", stringify(*config_params.load_environment))
            .with_setenv("PALUDIS_SKIP_INHERIT", "yes");

    return result;
}

EbuildConfigCommand::EbuildConfigCommand(const EbuildCommandParams & p,
        const EbuildConfigCommandParams & f) :
    EbuildCommand(p),
    config_params(f)
{
}

WriteVDBEntryCommand::WriteVDBEntryCommand(const WriteVDBEntryParams & p) :
    params(p)
{
}

void
WriteVDBEntryCommand::operator() ()
{
    std::string ebuild_cmd(getenv_with_default("PALUDIS_EBUILD_DIR", LIBEXECDIR "/paludis") +
            "/write_vdb_entry.bash '" +
            stringify(params.output_directory) + "' '" +
            stringify(params.environment_file) + "'");

    tr1::shared_ptr<const FSEntryCollection> syncers_dirs(params.environment->syncers_dirs());
    tr1::shared_ptr<const FSEntryCollection> bashrc_files(params.environment->bashrc_files());
    tr1::shared_ptr<const FSEntryCollection> fetchers_dirs(params.environment->fetchers_dirs());
    tr1::shared_ptr<const FSEntryCollection> hook_dirs(params.environment->hook_dirs());

    Command cmd(Command(ebuild_cmd)
            .with_setenv("PKGMANAGER", PALUDIS_PACKAGE "-" + stringify(PALUDIS_VERSION_MAJOR) + "." +
                stringify(PALUDIS_VERSION_MINOR) + "." +
                stringify(PALUDIS_VERSION_MICRO) +
                (std::string(PALUDIS_SUBVERSION_REVISION).empty() ?
                 std::string("") : "-r" + std::string(PALUDIS_SUBVERSION_REVISION)))
            .with_setenv("PALUDIS_CONFIG_DIR", SYSCONFDIR "/paludis/")
            .with_setenv("PALUDIS_BASHRC_FILES", join(bashrc_files->begin(), bashrc_files->end(), " "))
            .with_setenv("PALUDIS_HOOK_DIRS", join(hook_dirs->begin(), hook_dirs->end(), " "))
            .with_setenv("PALUDIS_FETCHERS_DIRS", join(fetchers_dirs->begin(), fetchers_dirs->end(), " "))
            .with_setenv("PALUDIS_SYNCERS_DIRS", join(syncers_dirs->begin(), syncers_dirs->end(), " "))
            .with_setenv("PALUDIS_COMMAND", params.environment->paludis_command())
            .with_setenv("PALUDIS_EBUILD_LOG_LEVEL", stringify(Log::get_instance()->log_level()))
            .with_setenv("PALUDIS_EBUILD_DIR", getenv_with_default("PALUDIS_EBUILD_DIR", LIBEXECDIR "/paludis")));

    if (0 != (run_command(cmd)))
        throw PackageInstallActionError("Write VDB Entry command failed");
}

VDBPostMergeCommand::VDBPostMergeCommand(const VDBPostMergeCommandParams & p) :
    params(p)
{
}

void
VDBPostMergeCommand::operator() ()
{
    if (! getenv_with_default("PALUDIS_NO_GLOBAL_HOOKS", "").empty())
        return;

#ifdef HAVE_GNU_LDCONFIG
    std::string ebuild_cmd("ldconfig -r '" + stringify(params.root) + "'");
#else
    std::string ebuild_cmd("ldconfig -elf -i -f '" + stringify(params.root) +
            "var/run/ld-elf.so.hints' '" + stringify(params.root) + "etc/ld.so.conf'");
#endif

    if (0 != (run_command(ebuild_cmd)))
        throw PackageInstallActionError("VDB Entry post merge commands failed");
}

std::string
EbuildPretendCommand::commands() const
{
    return params.commands;
}

bool
EbuildPretendCommand::failure()
{
    return false;
}

Command
EbuildPretendCommand::extend_command(const Command & cmd)
{
    Command result(Command(cmd)
            .with_stdout_prefix(stringify(params.package_id->name().package) + "-" +
                stringify(params.package_id->version()) + "> ")
            .with_stderr_prefix(stringify(params.package_id->name().package) + "-" +
                stringify(params.package_id->version()) + "> ")
            .with_prefix_discard_blank_output()
            .with_prefix_blank_lines()
            .with_setenv("USE", pretend_params.use)
            .with_setenv("USE_EXPAND", pretend_params.use_expand)
            .with_setenv("ROOT", pretend_params.root)
            .with_setenv("PALUDIS_PROFILE_DIR", stringify(*pretend_params.profiles->begin()))
            .with_setenv("PALUDIS_PROFILE_DIRS", join(pretend_params.profiles->begin(),
                    pretend_params.profiles->end(), " ")));

    for (AssociativeCollection<std::string, std::string>::Iterator
            i(pretend_params.expand_vars->begin()),
            j(pretend_params.expand_vars->end()) ; i != j ; ++i)
        result.with_setenv(i->first, i->second);

    result.with_uid_gid(params.environment->reduced_uid(), params.environment->reduced_gid());

    return result;
}

EbuildPretendCommand::EbuildPretendCommand(const EbuildCommandParams & p,
        const EbuildPretendCommandParams & f) :
    EbuildCommand(p),
    pretend_params(f)
{
}
