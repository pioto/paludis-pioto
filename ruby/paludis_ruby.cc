/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2006, 2007 Ciaran McCreesh <ciaranm@ciaranm.org>
 * Copyright (c) 2006, 2007 Richard Brown <rbrown@gentoo.org>
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

#include <paludis/paludis.hh>
#include <paludis_ruby.hh>
#include <paludis/config_file.hh>
#include <paludis/dep_list/exceptions.hh>
#include <ruby.h>
#include <list>
#include <ctype.h>

#ifdef ENABLE_RUBY_QA
#include <paludis/qa/qa_environment.hh>
#endif

using namespace paludis;
using namespace paludis::ruby;

#define RUBY_FUNC_CAST(x) reinterpret_cast<VALUE (*)(...)>(x)

namespace paludis
{
    template<>
    struct Implementation<RegisterRubyClass>
    {
        std::list<void (*)()> funcs;
    };
}

namespace
{
    static VALUE c_paludis_module;
    static VALUE c_name_error;
    static VALUE c_set_name_error;
    static VALUE c_category_name_part_error;
    static VALUE c_package_name_part_error;
    static VALUE c_bad_version_spec_error;
    static VALUE c_package_dep_spec_error;
    static VALUE c_package_database_error;
    static VALUE c_package_database_lookup_error;
    static VALUE c_ambiguous_package_name_error;
    static VALUE c_no_such_package_error;
    static VALUE c_no_such_repository_error;
    static VALUE c_dep_string_error;
    static VALUE c_dep_string_parse_error;
    static VALUE c_dep_string_nesting_error;
    static VALUE c_configuration_error;
    static VALUE c_config_file_error;
    static VALUE c_dep_list_error;
    static VALUE c_all_masked_error;
    static VALUE c_block_error;
    static VALUE c_circular_dependency_error;
    static VALUE c_use_requirements_not_met_error;
    static VALUE c_downgrade_not_allowed_error;
    static VALUE c_no_destination_error;

    static VALUE c_environment;
    static VALUE c_no_config_environment;

#ifdef ENABLE_RUBY_QA
    static VALUE c_paludis_qa_module;
    static VALUE c_profiles_desc_error;
    static VALUE c_no_such_file_check_type_error;
    static VALUE c_no_such_package_dir_check_type_error;
    static VALUE c_no_such_ebuild_check_type_error;
#endif

    /* Document-method: match_package
     *
     * call-seq:
     *     match_package (env, spec, target)
     *
     * Return whether the specified spec matches the specified target.
     *
     */
    VALUE paludis_match_package(VALUE, VALUE en, VALUE a, VALUE t)
    {
        try
        {
            Environment * env = value_to_environment_data(en)->env_ptr;
            std::tr1::shared_ptr<const PackageDepSpec> spec = value_to_package_dep_spec(a);
            PackageDatabaseEntry target = value_to_package_database_entry(t);
            return match_package(*env, *spec, target) ? Qtrue : Qfalse;
        }
        catch (const std::exception & e)
        {
            exception_to_ruby_exception(e);
        }

    }
}

RegisterRubyClass::RegisterRubyClass() :
    PrivateImplementationPattern<RegisterRubyClass>(new Implementation<RegisterRubyClass>)
{
}

RegisterRubyClass::~RegisterRubyClass()
{
}

void
RegisterRubyClass::execute() const
{
    for (std::list<void (*)()>::const_iterator f(_imp->funcs.begin()), f_end(_imp->funcs.end()) ;
            f != f_end ; ++f)
        (*f)();
}

RegisterRubyClass::Register::Register(void (* f)())
{
    RegisterRubyClass::get_instance()->_imp->funcs.push_back(f);
}

void paludis::ruby::exception_to_ruby_exception(const std::exception & ee)
{
    if (0 != dynamic_cast<const paludis::InternalError *>(&ee))
        rb_raise(rb_eRuntimeError, "Unexpected paludis::InternalError: %s (%s)",
                dynamic_cast<const paludis::InternalError *>(&ee)->message().c_str(), ee.what());
    else if (0 != dynamic_cast<const paludis::BadVersionSpecError *>(&ee))
        rb_raise(c_bad_version_spec_error, dynamic_cast<const paludis::BadVersionSpecError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::SetNameError *>(&ee))
        rb_raise(c_set_name_error, dynamic_cast<const paludis::SetNameError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::PackageNamePartError *>(&ee))
        rb_raise(c_package_name_part_error, dynamic_cast<const paludis::PackageNamePartError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::CategoryNamePartError *>(&ee))
        rb_raise(c_category_name_part_error, dynamic_cast<const paludis::CategoryNamePartError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::NameError *>(&ee))
        rb_raise(c_name_error, dynamic_cast<const paludis::NameError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::PackageDepSpecError *>(&ee))
        rb_raise(c_package_dep_spec_error, dynamic_cast<const paludis::PackageDepSpecError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::NoSuchRepositoryError *>(&ee))
        rb_raise(c_no_such_repository_error, dynamic_cast<const paludis::NoSuchRepositoryError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::AmbiguousPackageNameError *>(&ee))
        rb_raise(c_ambiguous_package_name_error, dynamic_cast<const paludis::AmbiguousPackageNameError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::NoSuchPackageError *>(&ee))
        rb_raise(c_no_such_package_error, dynamic_cast<const paludis::NoSuchPackageError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::PackageDatabaseLookupError *>(&ee))
        rb_raise(c_package_database_lookup_error, dynamic_cast<const paludis::PackageDatabaseLookupError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::PackageDatabaseError *>(&ee))
        rb_raise(c_package_database_error, dynamic_cast<const paludis::PackageDatabaseError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::DepStringNestingError *>(&ee))
        rb_raise(c_dep_string_nesting_error, dynamic_cast<const paludis::DepStringNestingError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::DepStringParseError *>(&ee))
        rb_raise(c_dep_string_parse_error, dynamic_cast<const paludis::DepStringParseError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::DepStringError *>(&ee))
        rb_raise(c_dep_string_error, dynamic_cast<const paludis::DepStringError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::AllMaskedError *>(&ee))
    {
        VALUE ex_args[2];
        ex_args[0] = rb_str_new2(dynamic_cast<const paludis::AllMaskedError *>(&ee)->message().c_str());
        ex_args[1] = rb_str_new2(stringify(dynamic_cast<const paludis::AllMaskedError *>(&ee)->query()).c_str());
        rb_exc_raise(rb_class_new_instance(2, ex_args, c_all_masked_error));
    }
    else if (0 != dynamic_cast<const paludis::BlockError *>(&ee))
        rb_raise(c_block_error, dynamic_cast<const paludis::BlockError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::CircularDependencyError *>(&ee))
        rb_raise(c_circular_dependency_error, dynamic_cast<const paludis::CircularDependencyError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::UseRequirementsNotMetError *>(&ee))
        rb_raise(c_use_requirements_not_met_error, dynamic_cast<const paludis::UseRequirementsNotMetError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::DowngradeNotAllowedError *>(&ee))
        rb_raise(c_downgrade_not_allowed_error, dynamic_cast<const paludis::DowngradeNotAllowedError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::NoDestinationError *>(&ee))
        rb_raise(c_no_destination_error, dynamic_cast<const paludis::NoDestinationError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::DepListError *>(&ee))
        rb_raise(c_dep_list_error, dynamic_cast<const paludis::DepListError *>(&ee)->message().c_str());
#ifdef ENABLE_RUBY_QA
    else if (0 != dynamic_cast<const paludis::qa::NoSuchFileCheckTypeError *>(&ee))
        rb_raise(c_no_such_file_check_type_error, dynamic_cast<const paludis::qa::NoSuchFileCheckTypeError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::qa::NoSuchPackageDirCheckTypeError *>(&ee))
        rb_raise(c_no_such_package_dir_check_type_error, dynamic_cast<const paludis::qa::NoSuchPackageDirCheckTypeError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::qa::NoSuchEbuildCheckTypeError *>(&ee))
        rb_raise(c_no_such_ebuild_check_type_error, dynamic_cast<const paludis::qa::NoSuchEbuildCheckTypeError *>(&ee)->message().c_str());
#endif
    else if (0 != dynamic_cast<const paludis::ConfigFileError *>(&ee))
        rb_raise(c_config_file_error, dynamic_cast<const paludis::ConfigFileError *>(&ee)->message().c_str());
    else if (0 != dynamic_cast<const paludis::ConfigurationError *>(&ee))
        rb_raise(c_configuration_error, dynamic_cast<const paludis::ConfigurationError *>(&ee)->message().c_str());

    else if (0 != dynamic_cast<const paludis::Exception *>(&ee))
        rb_raise(rb_eRuntimeError, "Caught paludis::Exception: %s (%s)",
                dynamic_cast<const paludis::Exception *>(&ee)->message().c_str(), ee.what());
    else
        rb_raise(rb_eRuntimeError, "Unexpected std::exception: (%s)", ee.what());
}

std::string
paludis::ruby::value_case_to_RubyCase(const std::string & s)
{
    if (s.empty())
        return s;

    bool upper_next(true);
    std::string result;
    for (std::string::size_type p(0), p_end(s.length()) ; p != p_end ; ++p)
    {
        if ('_' == s[p] || ' ' == s[p])
            upper_next = true;
        else if (upper_next)
        {
            result.append(std::string(1, toupper(s[p])));
            upper_next = false;
        }
        else
            result.append(std::string(1, s[p]));
    }

    return result;
}

VALUE
paludis::ruby::paludis_module()
{
    return c_paludis_module;
}

VALUE
paludis::ruby::environment_class()
{
    return c_environment;
}

VALUE
paludis::ruby::no_config_environment_class()
{
    return c_no_config_environment;
}

#ifdef ENABLE_RUBY_QA
VALUE
paludis::ruby::paludis_qa_module()
{
    return c_paludis_qa_module;
}
#endif

static VALUE
has_query_property_error_init(int argc, VALUE* argv, VALUE self)
{
    VALUE query;

    query = (argc > 1) ? argv[--argc] : Qnil;
    rb_call_super(argc, argv);
    rb_iv_set(self, "query", query);
    return self;
}

/*
 * call-seq:
 *     query -> String or Nil
 *
 * Our query.
 */
VALUE
has_query_property_error_query(VALUE self)
{
    return rb_attr_get(self, rb_intern("query"));
}

void paludis::ruby::init()
{
    /*
     * Document-module: Paludis
     *
     * <b>Paludis</b> is the other package mangler, this is the doc to the ruby binding. The C++ library
     * documentation[http://paludis.pioto.org/doxygen/html/] may also help.
     *
     */
    c_paludis_module = rb_define_module("Paludis");
    c_environment = rb_define_class_under(paludis_module(), "Environment", rb_cObject);
    c_no_config_environment = rb_define_class_under(paludis_module(), "NoConfigEnvironment", c_environment);
    c_name_error = rb_define_class_under(c_paludis_module, "NameError", rb_eRuntimeError);
    c_set_name_error = rb_define_class_under(c_paludis_module, "SetNameError", c_name_error);
    c_category_name_part_error = rb_define_class_under(c_paludis_module, "CategoryNamePartError", c_name_error);
    c_package_name_part_error = rb_define_class_under(c_paludis_module, "PackageNamePartError", c_name_error);
    c_bad_version_spec_error = rb_define_class_under(c_paludis_module, "BadVersionSpecError", c_name_error);
    c_package_dep_spec_error = rb_define_class_under(c_paludis_module, "PackageDepSpecError", rb_eRuntimeError);
    c_package_database_error = rb_define_class_under(c_paludis_module, "PackageDatabaseError", rb_eRuntimeError);
    c_package_database_lookup_error = rb_define_class_under(c_paludis_module, "PackageDatabaseLookupError", c_package_database_error);
    c_ambiguous_package_name_error = rb_define_class_under(c_paludis_module, "AmbiguousPackageNameError", c_package_database_lookup_error);
    c_no_such_package_error = rb_define_class_under(c_paludis_module, "NoSuchPackageError", c_package_database_lookup_error);
    c_no_such_repository_error = rb_define_class_under(c_paludis_module, "NoSuchRepositoryError", c_package_database_lookup_error);
    c_dep_string_error = rb_define_class_under(c_paludis_module, "DepStringError", rb_eRuntimeError);
    c_dep_string_parse_error = rb_define_class_under(c_paludis_module, "DepStringParseError", c_dep_string_error);
    c_dep_string_nesting_error = rb_define_class_under(c_paludis_module, "DepStringNestingError", c_dep_string_parse_error);
    c_configuration_error = rb_define_class_under(c_paludis_module, "ConfigurationError", rb_eRuntimeError);
    c_config_file_error = rb_define_class_under(c_paludis_module, "ConfigFileError", c_configuration_error);
    c_dep_list_error = rb_define_class_under(c_paludis_module, "DepListError", rb_eRuntimeError);

    /*
     * Document-class: Paludis::AllMaskedError
     *
     * Thrown if all versions of a particular spec are masked.
     */
    c_all_masked_error = rb_define_class_under(c_paludis_module, "AllMaskedError", c_dep_list_error);
    rb_define_method(c_all_masked_error, "initialize", RUBY_FUNC_CAST(&has_query_property_error_init), -1);
    rb_define_method(c_all_masked_error, "query", RUBY_FUNC_CAST(&has_query_property_error_query), 0);

    /*
     * Document-class: Paludis::BlockError
     *
     * Thrown if a block is encountered.
     */
    c_block_error = rb_define_class_under(c_paludis_module, "BlockError", c_dep_list_error);

    /*
     * Document-class: Paludis::CircularDependencyError
     *
     * Thrown if a circular dependency is encountered.
     */
    c_circular_dependency_error = rb_define_class_under(c_paludis_module, "CircularDependencyError", c_dep_list_error);

    /*
     * Document-class: Paludis::UseRequirementsNotMetError
     *
     * Thrown if all versions of a particular spec are masked, but would not be if use requirements were not in effect.
     */
    c_use_requirements_not_met_error = rb_define_class_under(c_paludis_module, "UseRequirementsNotMetError", c_dep_list_error);
    rb_define_method(c_use_requirements_not_met_error, "initialize", RUBY_FUNC_CAST(&has_query_property_error_init), -1);
    rb_define_method(c_use_requirements_not_met_error, "query", RUBY_FUNC_CAST(&has_query_property_error_query), 0);
    c_downgrade_not_allowed_error = rb_define_class_under(c_paludis_module, "DowngradeNotAllowedError", c_dep_list_error);
    c_no_destination_error = rb_define_class_under(c_paludis_module, "NoDestinationError", c_dep_list_error);

    rb_define_module_function(c_paludis_module, "match_package", RUBY_FUNC_CAST(&paludis_match_package), 3);

    rb_define_const(c_paludis_module, "Version", rb_str_new2((stringify(PALUDIS_VERSION_MAJOR) + "."
                    + stringify(PALUDIS_VERSION_MINOR) + "." + stringify(PALUDIS_VERSION_MICRO)).c_str()));
#ifdef ENABLE_RUBY_QA
    c_paludis_qa_module = rb_define_module_under(c_paludis_module,"QA");
    c_profiles_desc_error = rb_define_class_under(c_paludis_qa_module, "ProfilesDescError", c_configuration_error);
    c_no_such_file_check_type_error = rb_define_class_under(c_paludis_qa_module, "NoSuchFileCheckTypeError", rb_eTypeError);
    c_no_such_package_dir_check_type_error = rb_define_class_under(c_paludis_qa_module, "NoSuchPackageDirCheckTypeError", rb_eTypeError);
    c_no_such_ebuild_check_type_error = rb_define_class_under(c_paludis_qa_module, "NoSuchEbuildCheckTypeError", rb_eTypeError);
#endif
    RegisterRubyClass::get_instance()->execute();
}

