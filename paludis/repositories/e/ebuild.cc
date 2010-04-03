/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2006, 2007, 2008, 2009, 2010 Ciaran McCreesh
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
#include <paludis/repositories/e/eapi.hh>
#include <paludis/repositories/e/dep_parser.hh>
#include <paludis/repositories/e/pipe_command_handler.hh>
#include <paludis/repositories/e/dependencies_rewriter.hh>

#include <paludis/util/system.hh>
#include <paludis/util/strip.hh>
#include <paludis/util/log.hh>
#include <paludis/util/sequence.hh>
#include <paludis/util/options.hh>
#include <paludis/util/map.hh>
#include <paludis/util/join.hh>
#include <paludis/util/tokeniser.hh>
#include <paludis/util/wrapped_forward_iterator.hh>
#include <paludis/util/wrapped_output_iterator.hh>
#include <paludis/util/iterator_funcs.hh>
#include <paludis/util/destringify.hh>
#include <paludis/util/indirect_iterator-impl.hh>
#include <paludis/util/set.hh>
#include <paludis/util/cookie.hh>
#include <paludis/output_manager.hh>

#include <paludis/about.hh>
#include <paludis/environment.hh>
#include <paludis/util/config_file.hh>
#include <paludis/package_id.hh>
#include <paludis/metadata_key.hh>
#include <paludis/action.hh>
#include <paludis/elike_choices.hh>

#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <tr1/functional>
#include <list>
#include <set>

#include "config.h"

/** \file
 * Implementation for ebuild.hh things.
 *
 * \ingroup grpebuildinterface
 */

using namespace paludis;
using namespace paludis::erepository;

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

namespace
{
    std::string get_jobs(const std::tr1::shared_ptr<const PackageID> & id)
    {
        std::tr1::shared_ptr<const ChoiceValue> choice;
        if (id->choices_key())
            choice = id->choices_key()->value()->find_by_name_with_prefix(
                    ELikeJobsChoiceValue::canonical_name_with_prefix());
        if (choice && choice->enabled())
            return choice->parameter();
        else
            return "";
    }

    bool get_trace(const std::tr1::shared_ptr<const PackageID> & id)
    {
        std::tr1::shared_ptr<const ChoiceValue> choice;
        if (id->choices_key())
            choice = id->choices_key()->value()->find_by_name_with_prefix(
                    ELikeTraceChoiceValue::canonical_name_with_prefix());
        return choice && choice->enabled();
    }
}

bool
EbuildCommand::operator() ()
{
    Context context("When running an ebuild command on '" + stringify(*params.package_id()) + "':");

    Command cmd(getenv_with_default("PALUDIS_EBUILD_DIR", LIBEXECDIR "/paludis") +
            "/ebuild.bash '" + ebuild_file() + "' " + commands());

    if (! params.package_id()->eapi()->supported())
        throw InternalError(PALUDIS_HERE, "Tried to run EbuildCommand on an unsupported EAPI");

    if (params.clearenv())
        cmd.with_clearenv();

    if (params.sydbox())
        cmd.with_sydbox();
    else if (params.sandbox())
        cmd.with_sandbox();

    if (params.userpriv())
    {
        if (params.package_id()->eapi()->supported()->userpriv_cannot_use_root())
        {
            if (0 == params.environment()->reduced_uid() || 0 == params.environment()->reduced_gid())
                if (getenv_with_default("PALUDIS_BYPASS_USERPRIV_CHECKS", "").empty())
                    throw ActionFailedError("Need to be able to use non-0 user and group for userpriv for '" +
                            stringify(*params.package_id()) + "'");
        }
        cmd.with_uid_gid(params.environment()->reduced_uid(), params.environment()->reduced_gid());
    }

    using namespace std::tr1::placeholders;

    cmd.with_pipe_command_handler("PALUDIS_PIPE_COMMAND", std::tr1::bind(&pipe_command_handler, params.environment(),
                params.package_id(), _1, params.maybe_output_manager()), params.builddir());

    std::tr1::shared_ptr<const FSEntrySequence> syncers_dirs(params.environment()->syncers_dirs());
    std::tr1::shared_ptr<const FSEntrySequence> bashrc_files(params.environment()->bashrc_files());
    std::tr1::shared_ptr<const FSEntrySequence> fetchers_dirs(params.environment()->fetchers_dirs());
    std::tr1::shared_ptr<const FSEntrySequence> hook_dirs(params.environment()->hook_dirs());

    cmd = extend_command(cmd
            .with_setenv("PV", stringify(params.package_id()->version().remove_revision()))
            .with_setenv("PR", stringify(params.package_id()->version().revision_only()))
            .with_setenv("PN", stringify(params.package_id()->name().package()))
            .with_setenv("PVR", stringify(params.package_id()->version()))
            .with_setenv("CATEGORY", stringify(params.package_id()->name().category()))
            .with_setenv("REPOSITORY", stringify(params.package_id()->repository()->name()))
            .with_setenv("EAPI", stringify(params.package_id()->eapi()->exported_name()))
            .with_setenv("PKGMANAGER", PALUDIS_PACKAGE "-" + stringify(PALUDIS_VERSION_MAJOR) + "." +
                stringify(PALUDIS_VERSION_MINOR) + "." +
                stringify(PALUDIS_VERSION_MICRO) + stringify(PALUDIS_VERSION_SUFFIX) +
                (std::string(PALUDIS_GIT_HEAD).empty() ?
                 std::string("") : "-git-" + std::string(PALUDIS_GIT_HEAD)))
            .with_setenv("PALUDIS_TMPDIR", stringify(params.builddir()))
            .with_setenv("PALUDIS_PACKAGE_BUILDDIR", stringify(params.package_builddir()))
            .with_setenv("PALUDIS_CONFIG_DIR", SYSCONFDIR "/paludis/")
            .with_setenv("PALUDIS_BASHRC_FILES", join(bashrc_files->begin(), bashrc_files->end(), " "))
            .with_setenv("PALUDIS_HOOK_DIRS", join(hook_dirs->begin(), hook_dirs->end(), " "))
            .with_setenv("PALUDIS_FETCHERS_DIRS", join(fetchers_dirs->begin(), fetchers_dirs->end(), " "))
            .with_setenv("PALUDIS_SYNCERS_DIRS", join(syncers_dirs->begin(), syncers_dirs->end(), " "))
            .with_setenv("PALUDIS_COMMAND", params.environment()->paludis_command())
            .with_setenv("PALUDIS_REDUCED_GID", stringify(params.environment()->reduced_gid()))
            .with_setenv("PALUDIS_REDUCED_UID", stringify(params.environment()->reduced_uid()))
            .with_setenv("PALUDIS_EBUILD_LOG_LEVEL", stringify(Log::get_instance()->log_level()))
            .with_setenv("PALUDIS_EBUILD_DIR", getenv_with_default("PALUDIS_EBUILD_DIR", LIBEXECDIR "/paludis"))
            .with_setenv("PALUDIS_UTILITY_PATH_SUFFIXES",
                    params.package_id()->eapi()->supported()->ebuild_options()->utility_path_suffixes())
            .with_setenv("PALUDIS_EBUILD_MODULE_SUFFIXES",
                    params.package_id()->eapi()->supported()->ebuild_options()->ebuild_module_suffixes())
            .with_setenv("PALUDIS_NON_EMPTY_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->non_empty_variables())
            .with_setenv("PALUDIS_DIRECTORY_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->directory_variables())
            .with_setenv("PALUDIS_EBUILD_MUST_NOT_SET_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->ebuild_must_not_set_variables())
            .with_setenv("PALUDIS_ECLASS_MUST_NOT_SET_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->eclass_must_not_set_variables())
            .with_setenv("PALUDIS_SAVE_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->save_variables())
            .with_setenv("PALUDIS_SAVE_BASE_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->save_base_variables())
            .with_setenv("PALUDIS_SAVE_UNMODIFIABLE_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->save_unmodifiable_variables())
            .with_setenv("PALUDIS_DIRECTORY_IF_EXISTS_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->directory_if_exists_variables())
            .with_setenv("PALUDIS_SOURCE_MERGED_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->source_merged_variables())
            .with_setenv("PALUDIS_BRACKET_MERGED_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->bracket_merged_variables())
            .with_setenv("PALUDIS_BRACKET_MERGED_VARIABLES_ANNOTATABLE",
                    params.package_id()->eapi()->supported()->ebuild_options()->bracket_merged_variables_annotatable())
            .with_setenv("PALUDIS_BRACKET_MERGED_VARIABLES_ANNOTATION",
                    params.package_id()->eapi()->supported()->ebuild_options()->bracket_merged_variables_annotation())
            .with_setenv("PALUDIS_MUST_NOT_CHANGE_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->must_not_change_variables())
            .with_setenv("PALUDIS_MUST_NOT_SET_VARS_STARTING_WITH",
                    params.package_id()->eapi()->supported()->ebuild_options()->must_not_set_vars_starting_with())
            .with_setenv("PALUDIS_MUST_NOT_CHANGE_AFTER_SOURCE_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->must_not_change_after_source_variables())
            .with_setenv("PALUDIS_RDEPEND_DEFAULTS_TO_DEPEND",
                    params.package_id()->eapi()->supported()->ebuild_options()->rdepend_defaults_to_depend() ? "yes" : "")
            .with_setenv("PALUDIS_F_FUNCTION_PREFIX",
                    params.package_id()->eapi()->supported()->ebuild_options()->f_function_prefix())
            .with_setenv("PALUDIS_IGNORE_PIVOT_ENV_FUNCTIONS",
                    params.package_id()->eapi()->supported()->ebuild_options()->ignore_pivot_env_functions())
            .with_setenv("PALUDIS_IGNORE_PIVOT_ENV_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->ignore_pivot_env_variables())
            .with_setenv("PALUDIS_LOAD_MODULES",
                    params.package_id()->eapi()->supported()->ebuild_options()->load_modules())
            .with_setenv("PALUDIS_EBUILD_FUNCTIONS",
                    params.package_id()->eapi()->supported()->ebuild_options()->ebuild_functions())
            .with_setenv("PALUDIS_NO_S_WORKDIR_FALLBACK",
                    params.package_id()->eapi()->supported()->ebuild_options()->no_s_workdir_fallback() ? "yes" : "")
            .with_setenv("PALUDIS_BINARY_DISTDIR_VARIABLE",
                    params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_distdir())
            .with_setenv("PALUDIS_ECONF_EXTRA_OPTIONS",
                    params.package_id()->eapi()->supported()->tools_options()->econf_extra_options())
            .with_setenv("PALUDIS_UNPACK_UNRECOGNISED_IS_FATAL",
                    params.package_id()->eapi()->supported()->tools_options()->unpack_unrecognised_is_fatal() ? "yes" : "")
            .with_setenv("PALUDIS_UNPACK_FIX_PERMISSIONS",
                    params.package_id()->eapi()->supported()->tools_options()->unpack_fix_permissions() ? "yes" : "")
            .with_setenv("PALUDIS_UNPACK_SUFFIXES",
                    params.package_id()->eapi()->supported()->tools_options()->unpack_suffixes())
            .with_setenv("PALUDIS_DODOC_R",
                    params.package_id()->eapi()->supported()->tools_options()->dodoc_r() ? "yes" : "")
            .with_setenv("PALUDIS_DOINS_SYMLINK",
                    params.package_id()->eapi()->supported()->tools_options()->doins_symlink() ? "yes" : "")
            .with_setenv("PALUDIS_DOMAN_LANG_FILENAMES",
                    params.package_id()->eapi()->supported()->tools_options()->doman_lang_filenames() ? "yes" : "")
            .with_setenv("PALUDIS_DOSYM_NO_MKDIR",
                    params.package_id()->eapi()->supported()->tools_options()->dosym_mkdir() ? "" : "yes")
            .with_setenv("PALUDIS_FAILURE_IS_FATAL",
                    params.package_id()->eapi()->supported()->tools_options()->failure_is_fatal() ? "yes" : "")
            .with_setenv("PALUDIS_UNPACK_FROM_VAR",
                    params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_distdir())
            .with_setenv("PALUDIS_IMAGE_DIR_VAR",
                    params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_d())
            .with_setenv("PALUDIS_TEMP_DIR_VAR",
                    params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_t())
            .with_setenv("PALUDIS_NAME_VERSION_REVISION_VAR",
                    params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_pf())
            .with_setenv("PALUDIS_EBUILD_PHASE_VAR",
                    params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_ebuild_phase())
            .with_setenv("PALUDIS_PIPE_COMMANDS_SUPPORTED", "yes")
            .with_setenv("PALUDIS_PIPE_COMMAND_DELIM", "\2")
            .with_setenv("PALUDIS_SHELL_OPTIONS",
                    params.package_id()->eapi()->supported()->ebuild_options()->shell_options())
            )
        .with_setenv("SLOT", "")
        .with_setenv("PALUDIS_PROFILE_DIR", "")
        .with_setenv("PALUDIS_PROFILE_DIRS", "")
        .with_setenv("PALUDIS_PROFILES_DIRS", "")
        .with_setenv("ROOT", params.root());

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_kv().empty())
        cmd.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_kv(), kernel_version());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_portdir().empty())
        cmd.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_portdir(),
                stringify(params.portdir()));
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_distdir().empty())
        cmd.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_distdir(),
                        stringify(params.distdir()));
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_p().empty())
        cmd.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_p(),
                        stringify(params.package_id()->name().package()) + "-" +
                                stringify(params.package_id()->version().remove_revision()));
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_pf().empty())
        cmd.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_pf(),
                        stringify(params.package_id()->name().package()) + "-" +
                                stringify(params.package_id()->version()));
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_filesdir().empty())
        cmd.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_filesdir(),
                        stringify(params.files_dir()));

    if (! params.package_id()->eapi()->supported()->ebuild_metadata_variables()->iuse_effective()->name().empty())
        if (params.package_id()->raw_iuse_effective_key())
            cmd.with_setenv(params.package_id()->eapi()->supported()->ebuild_metadata_variables()->iuse_effective()->name(),
                    join(params.package_id()->raw_iuse_effective_key()->value()->begin(),
                        params.package_id()->raw_iuse_effective_key()->value()->end(), " "));

    if (params.package_id()->eapi()->supported()->ebuild_options()->support_eclasses())
        cmd
            .with_setenv("ECLASSDIR", stringify(*params.eclassdirs()->begin()))
            .with_setenv("ECLASSDIRS", join(params.eclassdirs()->begin(),
                        params.eclassdirs()->end(), " "));

    if (params.package_id()->eapi()->supported()->ebuild_options()->support_exlibs())
        cmd
            .with_setenv("EXLIBSDIRS", join(params.exlibsdirs()->begin(),
                        params.exlibsdirs()->end(), " "));

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_jobs().empty())
        cmd
            .with_setenv("PALUDIS_JOBS_VAR", params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_jobs())
            .with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_jobs(),
                    get_jobs(params.package_id()));

    cmd.with_setenv("PALUDIS_PREFIX_IMAGE_VAR", params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_ed());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_eprefix().empty())
        cmd.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_eprefix(), "");
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_eroot().empty())
        cmd.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_eroot(), params.root());

    cmd
        .with_setenv("PALUDIS_TRACE", get_trace(params.package_id()) ? "yes" : "");

    if (params.package_id()->eapi()->supported()->ebuild_options()->want_portage_emulation_vars())
        cmd = add_portage_vars(cmd);

    if (params.maybe_output_manager())
        cmd
            .with_captured_stderr_stream(&params.maybe_output_manager()->stderr_stream())
            .with_captured_stdout_stream(&params.maybe_output_manager()->stdout_stream())
            .with_ptys();

    if (do_run_command(cmd))
        return success();
    else
        return failure();
}

std::string
EbuildCommand::ebuild_file() const
{
    return stringify(params.ebuild_file());
}

Command
EbuildCommand::add_portage_vars(const Command & cmd) const
{
    return Command(cmd)
        .with_setenv("PORTAGE_ACTUAL_DISTDIR", stringify(params.distdir()))
        .with_setenv("PORTAGE_BASHRC", "/dev/null")
        .with_setenv("PORTAGE_BUILDDIR", stringify(params.package_builddir()))
        .with_setenv("PORTAGE_CALLER", params.environment()->paludis_command())
        .with_setenv("PORTAGE_GID", "0")
        .with_setenv("PORTAGE_INST_GID", "0")
        .with_setenv("PORTAGE_INST_UID", "0")
        .with_setenv("PORTAGE_MASTER_PID", stringify(::getpid()))
        .with_setenv("PORTAGE_NICENCESS", stringify(::getpriority(PRIO_PROCESS, 0)))
        .with_setenv("PORTAGE_TMPDIR", stringify(params.builddir()))
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
    return params.commands();
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
        .with_uid_gid(params.environment()->reduced_uid(), params.environment()->reduced_gid())
        ;
}

namespace
{
    std::string purdy(const std::string & s)
    {
        std::list<std::string> tokens;
        tokenise_whitespace(s, std::back_inserter(tokens));
        return join(tokens.begin(), tokens.end(), " \\n ");
    }
}

bool
EbuildMetadataCommand::do_run_command(const Command & cmd)
{
    bool ok(false);
    keys.reset(new Map<std::string, std::string>);

    std::string input;
    try
    {
        Context context("When running ebuild command to generate metadata for '" + stringify(*params.package_id()) + "':");

        std::stringstream prog, prog_err;
        Command real_cmd(cmd);
        int exit_status(run_command(real_cmd.with_captured_stdout_stream(&prog).with_captured_stderr_stream(&prog_err)));
        input.assign((std::istreambuf_iterator<char>(prog)), std::istreambuf_iterator<char>());
        std::stringstream input_stream(input);
        KeyValueConfigFile f(input_stream, KeyValueConfigFileOptions() + kvcfo_disallow_continuations + kvcfo_disallow_comments
                + kvcfo_disallow_space_around_equals + kvcfo_disallow_unquoted_values + kvcfo_disallow_source
                + kvcfo_disallow_variables + kvcfo_preserve_whitespace,
                &KeyValueConfigFile::no_defaults, &KeyValueConfigFile::no_transformation);

        std::copy(f.begin(), f.end(), keys->inserter());
        if (0 == exit_status)
            ok = true;

        captured_stderr = prog_err.str();
    }
    catch (const InternalError &)
    {
        throw;
    }
    catch (const Exception & e)
    {
        Log::get_instance()->message("e.ebuild.cache_failure", ll_warning, lc_context) << "Caught exception '"
                << e.message() << "' (" << e.what() << ") when generating cache for '"
                << *params.package_id() << "', input is '" << purdy(input) << "', stderr is '" << captured_stderr << "'";
    }

    if (ok)
        return true;
    else
    {
        Log::get_instance()->message("e.ebuild.cache_failure", ll_warning, lc_context) << "Could not generate cache for '"
            << *params.package_id() << "' stderr says '" << captured_stderr << "'";
        keys.reset(new Map<std::string, std::string>);
        keys->insert("EAPI", EAPIData::get_instance()->unknown_eapi()->name());

        return false;
    }
}

namespace
{
    std::string get(const std::tr1::shared_ptr<const Map<std::string, std::string> > & k, const std::string & s)
    {
        Map<std::string, std::string>::ConstIterator i(k->find(s));
        if (k->end() == i)
            return "";
        return i->second;
    }
}

void
EbuildMetadataCommand::load(const std::tr1::shared_ptr<const EbuildID> & id)
{
    Context context("When loading generated metadata for '" + stringify(*params.package_id()) + "':");

    if (! keys)
        throw InternalError(PALUDIS_HERE, "keys is 0");

    if (! id->eapi()->supported())
    {
        Log::get_instance()->message("e.ebuild.preload_eapi.unsupported", ll_debug, lc_context)
            << "ID pre-load EAPI '" << id->eapi()->name() << "' not supported";

        if (! captured_stderr.empty())
            id->load_captured_stderr("STDERR", "Captured stderr", mkt_normal, captured_stderr);

        return;
    }
    else
        Log::get_instance()->message("e.ebuild.preload_eapi.supported", ll_debug, lc_context)
            << "ID pre-load EAPI '" << id->eapi()->name() << "' is supported";

    std::string s;
    if (! ((s = get(keys, id->eapi()->supported()->ebuild_metadata_variables()->eapi()->name()))).empty())
        id->set_eapi(s);
    else
        id->set_eapi(id->e_repository()->params().eapi_when_unspecified());

    if (! id->eapi()->supported())
    {
        Log::get_instance()->message("e.ebuild.postload_eapi.unsupported", ll_debug, lc_context)
            << "ID post-load EAPI '" << id->eapi()->name() << "' not supported";
        if (! captured_stderr.empty())
            id->load_captured_stderr("STDERR", "Captured stderr", mkt_normal, captured_stderr);
        return;
    }
    else
        Log::get_instance()->message("e.ebuild.postload_eapi.supported", ll_debug, lc_context)
            << "ID post-load EAPI '" << id->eapi()->name() << "' is supported";

    if (! captured_stderr.empty())
        id->load_captured_stderr("STDERR", "Captured stderr", mkt_internal, captured_stderr);

    const EAPIEbuildMetadataVariables & m(*id->eapi()->supported()->ebuild_metadata_variables());

    if (! m.short_description()->name().empty())
        id->load_short_description(m.short_description()->name(), m.short_description()->description(), get(keys, m.short_description()->name()));

    if (! m.long_description()->name().empty())
    {
        std::string value(get(keys, m.long_description()->name()));
        if (! value.empty())
            id->load_long_description(m.long_description()->name(), m.long_description()->description(), value);
    }

    if (! m.dependencies()->name().empty())
    {
        DependenciesRewriter rewriter;
        std::string dep_s(get(keys, m.dependencies()->name()));
        parse_depend(dep_s, params.environment(), id, *id->eapi())->root()->accept(rewriter);
        id->load_raw_depend(m.dependencies()->name(), m.dependencies()->description(), dep_s);
        id->load_build_depend(m.dependencies()->name() + ".DEPEND", m.dependencies()->description() + " (build)", rewriter.depend(), true);
        id->load_run_depend(m.dependencies()->name() + ".RDEPEND", m.dependencies()->description() + " (run)", rewriter.rdepend(), true);
        id->load_post_depend(m.dependencies()->name() + ".PDEPEND", m.dependencies()->description() + " (post)", rewriter.pdepend(), true);
    }
    else
    {
        if (! m.build_depend()->name().empty())
            id->load_build_depend(m.build_depend()->name(), m.build_depend()->description(), get(keys, m.build_depend()->name()), false);

        if (! m.run_depend()->name().empty())
        {
            if (id->eapi()->supported()->ebuild_options()->rdepend_defaults_to_depend())
            {
                if (! get(keys, "PALUDIS_EBUILD_RDEPEND_WAS_SET").empty())
                    id->load_run_depend(m.run_depend()->name(), m.run_depend()->description(),
                            get(keys, m.run_depend()->name()), false);
                else
                    id->load_run_depend(m.run_depend()->name(), m.run_depend()->description(),
                            get(keys, "PALUDIS_EBUILD_DEPEND") + " " + get(keys, m.run_depend()->name()), false);
            }
            else
                id->load_run_depend(m.run_depend()->name(), m.run_depend()->description(),
                        get(keys, m.run_depend()->name()), false);
        }

        if (! m.pdepend()->name().empty())
            id->load_post_depend(m.pdepend()->name(), m.pdepend()->description(), get(keys, m.pdepend()->name()), false);
    }

    if (! m.slot()->name().empty())
    {
        try
        {
            Context c("When setting SLOT:");
            std::string slot(get(keys, m.slot()->name()));
            if (slot.empty())
            {
                Log::get_instance()->message("e.ebuild.no_slot", ll_qa, lc_context)
                    << "Package '" << *id << "' set SLOT=\"\", using SLOT=\"0\" instead";
                slot = "0";
            }
            id->load_slot(m.slot(), slot);
        }
        catch (const InternalError &)
        {
            throw;
        }
        catch (const Exception & e)
        {
            Log::get_instance()->message("e.ebuild.bad_slot", ll_warning, lc_context)
                << "Setting SLOT for '" << *id << "' failed due to exception '"
                << e.message() << "' (" << e.what() << ")";
            id->load_slot(m.slot(), "0");
        }
    }

    if (! m.src_uri()->name().empty())
        id->load_src_uri(m.src_uri(), get(keys, m.src_uri()->name()));

    if (! m.homepage()->name().empty())
        id->load_homepage(m.homepage()->name(), m.homepage()->description(), get(keys, m.homepage()->name()));

    if (! m.license()->name().empty())
        id->load_license(m.license(), get(keys, m.license()->name()));

    if (! m.provide()->name().empty())
        id->load_provide(m.provide()->name(), m.provide()->description(), get(keys, m.provide()->name()));

    if (! m.iuse()->name().empty())
        id->load_iuse(m.iuse()->name(), m.iuse()->description(), get(keys, m.iuse()->name()));

    if (! m.myoptions()->name().empty())
        id->load_myoptions(m.myoptions()->name(), m.myoptions()->description(), get(keys, m.myoptions()->name()));

    if (! m.inherited()->name().empty())
        id->load_inherited(m.inherited()->name(), m.inherited()->description(), get(keys, m.inherited()->name()));

    if (! m.keywords()->name().empty())
        id->load_keywords(m.keywords()->name(), m.keywords()->description(), get(keys, m.keywords()->name()));

    if (! m.restrictions()->name().empty())
        id->load_restrict(m.restrictions(), get(keys, m.restrictions()->name()));

    if (! m.properties()->name().empty())
        id->load_properties(m.properties(), get(keys, m.properties()->name()));

    if (! m.use()->name().empty())
        id->load_use(m.use()->name(), m.use()->description(), get(keys, m.use()->name()));

    if (! m.upstream_changelog()->name().empty())
    {
        std::string value(get(keys, m.upstream_changelog()->name()));
        if (! value.empty())
            id->load_upstream_changelog(m.upstream_changelog()->name(), m.upstream_changelog()->description(), value);
    }

    if (! m.upstream_documentation()->name().empty())
    {
        std::string value(get(keys, m.upstream_documentation()->name()));
        if (! value.empty())
            id->load_upstream_documentation(m.upstream_documentation()->name(), m.upstream_documentation()->description(), value);
    }

    if (! m.upstream_release_notes()->name().empty())
    {
        std::string value(get(keys, m.upstream_release_notes()->name()));
        if (! value.empty())
            id->load_upstream_release_notes(m.upstream_release_notes()->name(), m.upstream_release_notes()->description(), value);
    }

    if (! m.bugs_to()->name().empty())
    {
        std::string value(get(keys, m.bugs_to()->name()));
        if (! value.empty())
            id->load_bugs_to(m.bugs_to(), value);
    }

    if (! m.remote_ids()->name().empty())
    {
        std::string value(get(keys, m.remote_ids()->name()));
        if (! value.empty())
            id->load_remote_ids(m.remote_ids(), value);
    }

    if (! m.defined_phases()->name().empty())
    {
        std::set<std::string> defined_phases, raw_values, ebuild_functions;
        std::string raw_value(get(keys, "PALUDIS_DECLARED_FUNCTIONS"));
        tokenise_whitespace(raw_value, std::inserter(raw_values, raw_values.begin()));
        tokenise_whitespace(id->eapi()->supported()->ebuild_options()->ebuild_functions(),
                std::inserter(ebuild_functions, ebuild_functions.end()));

        for (std::set<std::string>::const_iterator f(ebuild_functions.begin()), f_end(ebuild_functions.end()) ;
                f != f_end ; ++f)
        {
            if (0 == f->compare(0, 8, "builtin_"))
                continue;

            if (raw_values.end() == raw_values.find(*f))
                continue;

            std::string p(*f);
            if (0 == p.compare(0, 4, "src_"))
                p.erase(0, 4);
            else if (0 == p.compare(0, 4, "pkg_"))
                p.erase(0, 4);
            else
                throw InternalError(PALUDIS_HERE, "Got strange phase function '" + p + "'");

            defined_phases.insert(p);
        }

        if (defined_phases.empty())
            id->load_defined_phases(m.defined_phases()->name(), m.defined_phases()->description(), "-");
        else
            id->load_defined_phases(m.defined_phases()->name(), m.defined_phases()->description(),
                    join(defined_phases.begin(), defined_phases.end(), " "));
    }
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
    return params.commands();
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
        .with_uid_gid(params.environment()->reduced_uid(), params.environment()->reduced_gid());
}

bool
EbuildVariableCommand::do_run_command(const Command & cmd)
{
    std::stringstream prog;
    Command real_cmd(cmd);
    int exit_status(run_command(real_cmd.with_captured_stdout_stream(&prog)));
    _result = strip_trailing_string(
            std::string((std::istreambuf_iterator<char>(prog)),
                std::istreambuf_iterator<char>()), "\n");

    return (0 == exit_status);
}

std::string
EbuildNoFetchCommand::commands() const
{
    return params.commands();
}

bool
EbuildNoFetchCommand::failure()
{
    throw ActionFailedError("Fetch failed for '" + stringify(*params.package_id()) + "'");
}

Command
EbuildNoFetchCommand::extend_command(const Command & cmd)
{
    Command result(Command(cmd)
            .with_setenv("PALUDIS_PROFILE_DIR", stringify(*fetch_params.profiles()->begin()))
            .with_setenv("PALUDIS_PROFILE_DIRS", join(fetch_params.profiles()->begin(),
                    fetch_params.profiles()->end(), " "))
            .with_setenv("PALUDIS_PROFILES_DIRS", join(fetch_params.profiles_with_parents()->begin(),
                    fetch_params.profiles_with_parents()->end(), " "))
            .with_setenv("PALUDIS_ARCHIVES_VAR",
                    params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_a()));

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_a().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_a(),
                fetch_params.a());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_aa().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_aa(),
                fetch_params.aa());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use(),
                fetch_params.use());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand(),
                fetch_params.use_expand());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden(),
                fetch_params.use_expand_hidden());

    for (Map<std::string, std::string>::ConstIterator
            i(fetch_params.expand_vars()->begin()),
            j(fetch_params.expand_vars()->end()) ; i != j ; ++i)
        result.with_setenv(i->first, i->second);

    return result;
}

EbuildNoFetchCommand::EbuildNoFetchCommand(const EbuildCommandParams & p,
        const EbuildNoFetchCommandParams & f) :
    EbuildCommand(p),
    fetch_params(f)
{
}

std::string
EbuildInstallCommand::commands() const
{
    return params.commands();
}

bool
EbuildInstallCommand::failure()
{
    throw ActionFailedError("Install failed for '" + stringify(*params.package_id()) + "'");
}

Command
EbuildInstallCommand::extend_command(const Command & cmd)
{
    Command result(Command(cmd)
            .with_setenv("PALUDIS_LOADSAVEENV_DIR", stringify(install_params.loadsaveenv_dir()))
            .with_setenv("PALUDIS_CONFIG_PROTECT", install_params.config_protect())
            .with_setenv("PALUDIS_CONFIG_PROTECT_MASK", install_params.config_protect_mask())
            .with_setenv("PALUDIS_PROFILE_DIR", stringify(*install_params.profiles()->begin()))
            .with_setenv("PALUDIS_PROFILE_DIRS", join(install_params.profiles()->begin(),
                                          install_params.profiles()->end(), " "))
            .with_setenv("PALUDIS_PROFILES_DIRS", join(install_params.profiles_with_parents()->begin(),
                    install_params.profiles_with_parents()->end(), " "))
            .with_setenv("PALUDIS_ARCHIVES_VAR",
                    params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_a())
            .with_setenv("SLOT", stringify(install_params.slot())));

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_a().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_a(),
                install_params.a());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_aa().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_aa(),
                install_params.aa());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use(),
                install_params.use());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand(),
                install_params.use_expand());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden(),
                install_params.use_expand_hidden());

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_replacing_ids().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_replacing_ids(),
                join(indirect_iterator(install_params.replacing_ids()->begin()),
                    indirect_iterator(install_params.replacing_ids()->end()), " "));

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_replacing_versions().empty())
    {
        std::string s;
        for (PackageIDSequence::ConstIterator i(install_params.replacing_ids()->begin()),
                i_end(install_params.replacing_ids()->end()) ;
                i != i_end ; ++i)
            if ((*i)->name() == params.package_id()->name())
            {
                if (! s.empty())
                    s.append(" ");
                s.append(stringify((*i)->version()));
            }

        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_replacing_versions(), s);
    }

    for (Map<std::string, std::string>::ConstIterator
            i(install_params.expand_vars()->begin()),
            j(install_params.expand_vars()->end()) ; i != j ; ++i)
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
    return params.commands();
}

std::string
EbuildUninstallCommand::ebuild_file() const
{
    return "-";
}

bool
EbuildUninstallCommand::failure()
{
    throw ActionFailedError("Uninstall failed for '" + stringify(*params.package_id()) + "'");
}

Command
EbuildUninstallCommand::extend_command(const Command & cmd)
{
    Command result(Command(cmd)
            .with_setenv("PALUDIS_LOADSAVEENV_DIR", stringify(uninstall_params.loadsaveenv_dir()))
            );

    if (uninstall_params.load_environment())
        result
            .with_setenv("PALUDIS_LOAD_ENVIRONMENT", stringify(*uninstall_params.load_environment()))
            .with_setenv("PALUDIS_SKIP_INHERIT", "yes");

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_replaced_by_id().empty())
        if (uninstall_params.replaced_by())
            result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_replaced_by_id(),
                stringify(*uninstall_params.replaced_by()));

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_replaced_by_version().empty())
        if (uninstall_params.replaced_by())
            result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_replaced_by_version(),
                stringify(uninstall_params.replaced_by()->version()));

    return result;
}

EbuildUninstallCommand::EbuildUninstallCommand(const EbuildCommandParams & p,
        const EbuildUninstallCommandParams & f) :
    EbuildCommand(p),
    uninstall_params(f)
{
}

std::string
EbuildConfigCommand::ebuild_file() const
{
    return "-";
}

std::string
EbuildConfigCommand::commands() const
{
    return params.commands();
}

bool
EbuildConfigCommand::failure()
{
    throw ActionFailedError("Configure failed for '" + stringify(*params.package_id()) + "'");
}

Command
EbuildConfigCommand::extend_command(const Command & cmd)
{
    Command result(cmd);

    if (config_params.load_environment())
        result
            .with_setenv("PALUDIS_LOAD_ENVIRONMENT", stringify(*config_params.load_environment()))
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
    using namespace std::tr1::placeholders;

    std::string ebuild_cmd(getenv_with_default("PALUDIS_EBUILD_DIR", LIBEXECDIR "/paludis") +
            "/write_vdb_entry.bash '" +
            stringify(params.output_directory()) + "' '" +
            stringify(params.environment_file()) + "'");

    std::tr1::shared_ptr<const FSEntrySequence> syncers_dirs(params.environment()->syncers_dirs());
    std::tr1::shared_ptr<const FSEntrySequence> bashrc_files(params.environment()->bashrc_files());
    std::tr1::shared_ptr<const FSEntrySequence> fetchers_dirs(params.environment()->fetchers_dirs());
    std::tr1::shared_ptr<const FSEntrySequence> hook_dirs(params.environment()->hook_dirs());

    Command cmd(Command(ebuild_cmd)
            .with_setenv("PKGMANAGER", PALUDIS_PACKAGE "-" + stringify(PALUDIS_VERSION_MAJOR) + "." +
                stringify(PALUDIS_VERSION_MINOR) + "." +
                stringify(PALUDIS_VERSION_MICRO) +
                (std::string(PALUDIS_GIT_HEAD).empty() ?
                 std::string("") : "-git-" + std::string(PALUDIS_GIT_HEAD)))
            .with_setenv("EAPI", stringify(params.package_id()->eapi()->exported_name()))
            .with_setenv("PALUDIS_CONFIG_DIR", SYSCONFDIR "/paludis/")
            .with_setenv("PALUDIS_BASHRC_FILES", join(bashrc_files->begin(), bashrc_files->end(), " "))
            .with_setenv("PALUDIS_HOOK_DIRS", join(hook_dirs->begin(), hook_dirs->end(), " "))
            .with_setenv("PALUDIS_FETCHERS_DIRS", join(fetchers_dirs->begin(), fetchers_dirs->end(), " "))
            .with_setenv("PALUDIS_SYNCERS_DIRS", join(syncers_dirs->begin(), syncers_dirs->end(), " "))
            .with_setenv("PALUDIS_COMMAND", params.environment()->paludis_command())
            .with_setenv("PALUDIS_EBUILD_LOG_LEVEL", stringify(Log::get_instance()->log_level()))
            .with_setenv("PALUDIS_EBUILD_DIR", getenv_with_default("PALUDIS_EBUILD_DIR", LIBEXECDIR "/paludis"))
            .with_setenv("PALUDIS_VDB_FROM_ENV_VARIABLES",
                params.package_id()->eapi()->supported()->ebuild_options()->vdb_from_env_variables())
            .with_setenv("PALUDIS_VDB_FROM_ENV_UNLESS_EMPTY_VARIABLES",
                params.package_id()->eapi()->supported()->ebuild_options()->vdb_from_env_unless_empty_variables())
            .with_setenv("PALUDIS_F_FUNCTION_PREFIX",
                params.package_id()->eapi()->supported()->ebuild_options()->f_function_prefix())
            .with_setenv("PALUDIS_IGNORE_PIVOT_ENV_FUNCTIONS",
                    params.package_id()->eapi()->supported()->ebuild_options()->ignore_pivot_env_functions())
            .with_setenv("PALUDIS_IGNORE_PIVOT_ENV_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->ignore_pivot_env_variables())
            .with_setenv("PALUDIS_EBUILD_MODULE_SUFFIXES",
                    params.package_id()->eapi()->supported()->ebuild_options()->ebuild_module_suffixes())
            .with_setenv("PALUDIS_EBUILD_PHASE_VAR",
                    params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_ebuild_phase())
            .with_pipe_command_handler("PALUDIS_PIPE_COMMAND", std::tr1::bind(&pipe_command_handler, params.environment(),
                        params.package_id(), _1, params.maybe_output_manager()), params.output_directory())
            );

    if (! params.package_id()->eapi()->supported()->ebuild_metadata_variables()->iuse_effective()->name().empty())
        if (params.package_id()->raw_iuse_effective_key())
            cmd.with_setenv(params.package_id()->eapi()->supported()->ebuild_metadata_variables()->iuse_effective()->name(),
                    join(params.package_id()->raw_iuse_effective_key()->value()->begin(),
                        params.package_id()->raw_iuse_effective_key()->value()->end(), " "));

    if (params.maybe_output_manager())
        cmd
            .with_captured_stderr_stream(&params.maybe_output_manager()->stderr_stream())
            .with_captured_stdout_stream(&params.maybe_output_manager()->stdout_stream())
            .with_ptys();

    std::string defined_phases(params.package_id()->eapi()->supported()->ebuild_metadata_variables()->defined_phases()->name());
    if (! defined_phases.empty())
        if (params.package_id()->defined_phases_key())
            cmd.with_setenv(defined_phases, join(params.package_id()->defined_phases_key()->value()->begin(),
                        params.package_id()->defined_phases_key()->value()->end(), " "));

    if (0 != (run_command(cmd)))
        throw ActionFailedError("Write VDB Entry command failed");
}

VDBPostMergeUnmergeCommand::VDBPostMergeUnmergeCommand(const VDBPostMergeUnmergeCommandParams & p) :
    params(p)
{
}

void
VDBPostMergeUnmergeCommand::operator() ()
{
    if (! getenv_with_default("PALUDIS_NO_GLOBAL_HOOKS", "").empty())
        return;

    Command cmd(getenv_with_default("PALUDIS_EBUILD_DIR", LIBEXECDIR "/paludis") +
            "/utils/wrapped_ldconfig '" + stringify(params.root()) + "'");

    if (0 != (run_command(cmd)))
        throw ActionFailedError("VDB Entry post merge commands failed");
}

std::string
EbuildPretendCommand::commands() const
{
    return params.commands();
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
            .with_stdout_prefix(stringify(params.package_id()->name().package()) + "-" +
                stringify(params.package_id()->version()) + "> ")
            .with_stderr_prefix(stringify(params.package_id()->name().package()) + "-" +
                stringify(params.package_id()->version()) + "> ")
            .with_prefix_discard_blank_output()
            .with_prefix_blank_lines()
            .with_setenv("PALUDIS_PROFILE_DIR", stringify(*pretend_params.profiles()->begin()))
            .with_setenv("PALUDIS_PROFILE_DIRS", join(pretend_params.profiles()->begin(),
                    pretend_params.profiles()->end(), " "))
            .with_setenv("PALUDIS_PROFILES_DIRS", join(pretend_params.profiles_with_parents()->begin(),
                    pretend_params.profiles_with_parents()->end(), " ")));

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use(),
                pretend_params.use());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand(),
                pretend_params.use_expand());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden(),
                pretend_params.use_expand_hidden());

    for (Map<std::string, std::string>::ConstIterator
            i(pretend_params.expand_vars()->begin()),
            j(pretend_params.expand_vars()->end()) ; i != j ; ++i)
        result.with_setenv(i->first, i->second);

    result.with_uid_gid(params.environment()->reduced_uid(), params.environment()->reduced_gid());

    return result;
}

EbuildPretendCommand::EbuildPretendCommand(const EbuildCommandParams & p,
        const EbuildPretendCommandParams & f) :
    EbuildCommand(p),
    pretend_params(f)
{
}

std::string
EbuildInfoCommand::ebuild_file() const
{
    if (info_params.use_ebuild_file())
        return stringify(params.ebuild_file());
    else
        return "-";
}

std::string
EbuildInfoCommand::commands() const
{
    return params.commands();
}

bool
EbuildInfoCommand::failure()
{
    throw ActionFailedError("Info command failed");
}

Command
EbuildInfoCommand::extend_command(const Command & cmd)
{
    std::string info_vars(join(info_params.info_vars()->begin(), info_params.info_vars()->end(), " "));

    Command result(Command(cmd)
            .with_stdout_prefix("        ")
            .with_stderr_prefix("        ")
            .with_prefix_discard_blank_output()
            .with_prefix_blank_lines()
            .with_setenv("PALUDIS_INFO_VARS", info_vars)
            .with_setenv("PALUDIS_PROFILE_DIR",
                info_params.profiles()->empty() ? std::string("") : stringify(*info_params.profiles()->begin()))
            .with_setenv("PALUDIS_PROFILE_DIRS", join(info_params.profiles()->begin(),
                    info_params.profiles()->end(), " "))
            .with_setenv("PALUDIS_PROFILES_DIRS", join(info_params.profiles_with_parents()->begin(),
                    info_params.profiles_with_parents()->end(), " ")));

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use(),
                info_params.use());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand(),
                info_params.use_expand());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden(),
                info_params.use_expand_hidden());

    for (Map<std::string, std::string>::ConstIterator
            i(info_params.expand_vars()->begin()),
            j(info_params.expand_vars()->end()) ; i != j ; ++i)
        result.with_setenv(i->first, i->second);

    result.with_uid_gid(params.environment()->reduced_uid(), params.environment()->reduced_gid());

    if (info_params.load_environment())
        result
            .with_setenv("PALUDIS_LOAD_ENVIRONMENT", stringify(*info_params.load_environment()))
            .with_setenv("PALUDIS_SKIP_INHERIT", "yes");

    return result;
}

EbuildInfoCommand::EbuildInfoCommand(const EbuildCommandParams & p,
        const EbuildInfoCommandParams & f) :
    EbuildCommand(p),
    info_params(f)
{
}

WriteBinaryEbuildCommand::WriteBinaryEbuildCommand(const WriteBinaryEbuildCommandParams & p) :
    params(p)
{
}

void
WriteBinaryEbuildCommand::operator() ()
{
    using namespace std::tr1::placeholders;

    if (! EAPIData::get_instance()->eapi_from_string("pbin-1+" + params.package_id()->eapi()->exported_name())->supported())
        throw ActionFailedError("Don't know how to write binary ebuilds using EAPI 'pbin-1+" +
                params.package_id()->eapi()->exported_name());

    std::string bindistfile(stringify(params.destination_repository()->name()) + "--" + stringify(params.package_id()->name().category())
            + "--" + stringify(params.package_id()->name().package()) + "-" + stringify(params.package_id()->version())
            + "--" + cookie());

    std::string ebuild_cmd(getenv_with_default("PALUDIS_EBUILD_DIR", LIBEXECDIR "/paludis") +
            "/write_binary_ebuild.bash '" +
            stringify(params.binary_ebuild_location()) + "' '" +
            stringify(params.binary_distdir() / bindistfile) + "' '" +
            stringify(params.environment_file()) + "' '" +
            stringify(params.image()) + "'");

    std::tr1::shared_ptr<const FSEntrySequence> syncers_dirs(params.environment()->syncers_dirs());
    std::tr1::shared_ptr<const FSEntrySequence> bashrc_files(params.environment()->bashrc_files());
    std::tr1::shared_ptr<const FSEntrySequence> fetchers_dirs(params.environment()->fetchers_dirs());
    std::tr1::shared_ptr<const FSEntrySequence> hook_dirs(params.environment()->hook_dirs());

    Command cmd(Command(ebuild_cmd)
            .with_setenv("PKGMANAGER", PALUDIS_PACKAGE "-" + stringify(PALUDIS_VERSION_MAJOR) + "." +
                stringify(PALUDIS_VERSION_MINOR) + "." +
                stringify(PALUDIS_VERSION_MICRO) +
                (std::string(PALUDIS_GIT_HEAD).empty() ?
                 std::string("") : "-git-" + std::string(PALUDIS_GIT_HEAD)))
            .with_setenv("EAPI", stringify(params.package_id()->eapi()->exported_name()))
            .with_setenv("PALUDIS_CONFIG_DIR", SYSCONFDIR "/paludis/")
            .with_setenv("PALUDIS_TMPDIR", stringify(params.builddir()))
            .with_setenv("PALUDIS_BASHRC_FILES", join(bashrc_files->begin(), bashrc_files->end(), " "))
            .with_setenv("PALUDIS_HOOK_DIRS", join(hook_dirs->begin(), hook_dirs->end(), " "))
            .with_setenv("PALUDIS_FETCHERS_DIRS", join(fetchers_dirs->begin(), fetchers_dirs->end(), " "))
            .with_setenv("PALUDIS_SYNCERS_DIRS", join(syncers_dirs->begin(), syncers_dirs->end(), " "))
            .with_setenv("PALUDIS_COMMAND", params.environment()->paludis_command())
            .with_setenv("PALUDIS_EBUILD_LOG_LEVEL", stringify(Log::get_instance()->log_level()))
            .with_setenv("PALUDIS_EBUILD_DIR", getenv_with_default("PALUDIS_EBUILD_DIR", LIBEXECDIR "/paludis"))
            .with_setenv("PALUDIS_BINARY_FROM_ENV_VARIABLES",
                params.package_id()->eapi()->supported()->ebuild_options()->binary_from_env_variables())
            .with_setenv("PALUDIS_F_FUNCTION_PREFIX",
                params.package_id()->eapi()->supported()->ebuild_options()->f_function_prefix())
            .with_setenv("PALUDIS_IGNORE_PIVOT_ENV_FUNCTIONS",
                params.package_id()->eapi()->supported()->ebuild_options()->ignore_pivot_env_functions())
            .with_setenv("PALUDIS_IGNORE_PIVOT_ENV_VARIABLES",
                    params.package_id()->eapi()->supported()->ebuild_options()->ignore_pivot_env_variables())
            .with_setenv("PALUDIS_BINARY_URI_PREFIX", params.destination_repository()->params().binary_uri_prefix())
            .with_setenv("PALUDIS_BINARY_KEYWORDS", params.destination_repository()->params().binary_keywords())
            .with_setenv("PALUDIS_BINARY_KEYWORDS_VARIABLE", EAPIData::get_instance()->eapi_from_string("pbin-1+"
                        + params.package_id()->eapi()->exported_name())->supported()->ebuild_metadata_variables()->keywords()->name())
            .with_setenv("PALUDIS_BINARY_DISTDIR_VARIABLE", EAPIData::get_instance()->eapi_from_string("pbin-1+"
                        + params.package_id()->eapi()->exported_name())->supported()->ebuild_environment_variables()->env_distdir())
            .with_setenv("PALUDIS_EBUILD_MODULE_SUFFIXES",
                    params.package_id()->eapi()->supported()->ebuild_options()->ebuild_module_suffixes())
            .with_setenv("PALUDIS_EBUILD_PHASE_VAR",
                    params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_ebuild_phase())
            .with_pipe_command_handler("PALUDIS_PIPE_COMMAND", std::tr1::bind(&pipe_command_handler, params.environment(),
                        params.package_id(), _1, params.maybe_output_manager()), params.builddir())
            );

    if (0 != (run_command(cmd)))
        throw ActionFailedError("Write binary command failed");
}

std::string
EbuildBadOptionsCommand::commands() const
{
    return params.commands();
}

bool
EbuildBadOptionsCommand::failure()
{
    return false;
}

Command
EbuildBadOptionsCommand::extend_command(const Command & cmd)
{
    Command result(Command(cmd)
            .with_setenv("PALUDIS_PROFILE_DIR", stringify(*bad_options_params.profiles()->begin()))
            .with_setenv("PALUDIS_PROFILE_DIRS", join(bad_options_params.profiles()->begin(),
                    bad_options_params.profiles()->end(), " "))
            .with_setenv("PALUDIS_PROFILES_DIRS", join(bad_options_params.profiles_with_parents()->begin(),
                    bad_options_params.profiles_with_parents()->end(), " "))
            .with_setenv("EX_UNMET_REQUIREMENTS", join(bad_options_params.unmet_requirements()->begin(),
                    bad_options_params.unmet_requirements()->end(), "\n"))
            );

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use(),
                bad_options_params.use());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand(),
                bad_options_params.use_expand());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden(),
                bad_options_params.use_expand_hidden());

    for (Map<std::string, std::string>::ConstIterator
            i(bad_options_params.expand_vars()->begin()),
            j(bad_options_params.expand_vars()->end()) ; i != j ; ++i)
        result.with_setenv(i->first, i->second);

    result.with_uid_gid(params.environment()->reduced_uid(), params.environment()->reduced_gid());

    return result;
}

EbuildBadOptionsCommand::EbuildBadOptionsCommand(const EbuildCommandParams & p,
        const EbuildBadOptionsCommandParams & f) :
    EbuildCommand(p),
    bad_options_params(f)
{
}

std::string
EbuildFetchExtraCommand::commands() const
{
    return params.commands();
}

bool
EbuildFetchExtraCommand::failure()
{
    throw ActionFailedError("Extra fetch failed for '" + stringify(*params.package_id()) + "'");
}

Command
EbuildFetchExtraCommand::extend_command(const Command & cmd)
{
    Command result(Command(cmd)
            .with_setenv("PALUDIS_LOADSAVEENV_DIR", stringify(fetch_extra_params.loadsaveenv_dir()))
            .with_setenv("PALUDIS_PROFILE_DIR", stringify(*fetch_extra_params.profiles()->begin()))
            .with_setenv("PALUDIS_PROFILE_DIRS", join(fetch_extra_params.profiles()->begin(),
                                          fetch_extra_params.profiles()->end(), " "))
            .with_setenv("PALUDIS_PROFILES_DIRS", join(fetch_extra_params.profiles_with_parents()->begin(),
                    fetch_extra_params.profiles_with_parents()->end(), " "))
            .with_setenv("PALUDIS_ARCHIVES_VAR",
                    params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_a())
            .with_setenv("SLOT", stringify(fetch_extra_params.slot()))
            );

    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_a().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_a(),
                fetch_extra_params.a());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_aa().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_aa(),
                fetch_extra_params.aa());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use(),
                fetch_extra_params.use());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand(),
                fetch_extra_params.use_expand());
    if (! params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden().empty())
        result.with_setenv(params.package_id()->eapi()->supported()->ebuild_environment_variables()->env_use_expand_hidden(),
                fetch_extra_params.use_expand_hidden());

    for (Map<std::string, std::string>::ConstIterator
            i(fetch_extra_params.expand_vars()->begin()),
            j(fetch_extra_params.expand_vars()->end()) ; i != j ; ++i)
        result.with_setenv(i->first, i->second);

    return result;
}

EbuildFetchExtraCommand::EbuildFetchExtraCommand(const EbuildCommandParams & p,
        const EbuildFetchExtraCommandParams & f) :
    EbuildCommand(p),
    fetch_extra_params(f)
{
}

