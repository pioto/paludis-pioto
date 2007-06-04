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

#ifndef PALUDIS_GUARD_PALUDIS_ENVIRONMENTS_PORTAGE_PORTAGE_ENVIRONMENT_HH
#define PALUDIS_GUARD_PALUDIS_ENVIRONMENTS_PORTAGE_PORTAGE_ENVIRONMENT_HH 1

#include <paludis/environment_implementation.hh>

namespace paludis
{
    namespace portage_environment
    {
        /**
         * Thrown if a configuration error occurs in a PortageEnvironment.
         *
         * \ingroup grpportageenvironment
         * \ingroup grpexceptions
         * \nosubgrouping
         */
        class PALUDIS_VISIBLE PortageEnvironmentConfigurationError :
            public ConfigurationError
        {
            public:
                ///\name Basic operations
                ///\{

                PortageEnvironmentConfigurationError(const std::string &) throw ();

                ///\}
        };
    }

    /**
     * Environment using Portage-like configuration files.
     *
     * \ingroup grpportageenvironment
     * \nosubgrouping
     */
    class PALUDIS_VISIBLE PortageEnvironment :
        public EnvironmentImplementation,
        private PrivateImplementationPattern<PortageEnvironment>
    {
        private:
            void _load_profile(const FSEntry &);
            void _add_virtuals_repository();
            void _add_installed_virtuals_repository();
            void _add_portdir_repository(const FSEntry &);
            void _add_portdir_overlay_repository(const FSEntry &);
            void _add_ebuild_repository(const FSEntry &, const std::string &,
                    const std::string &, int importance);
            void _add_vdb_repository();

            template<typename I_>
            void _load_lined_file(const FSEntry &, I_);

            template<typename I_>
            void _load_atom_file(const FSEntry &, I_, const std::string &);

        protected:
            virtual bool accept_keywords(tr1::shared_ptr<const KeywordNameCollection>, const PackageDatabaseEntry &) const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual bool accept_breaks_portage(const PackageDatabaseEntry &) const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual bool masked_by_user(const PackageDatabaseEntry &) const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual bool unmasked_by_user(const PackageDatabaseEntry &) const
                PALUDIS_ATTRIBUTE((warn_unused_result));

        public:
            ///\name Basic operations
            ///\{

            PortageEnvironment(const std::string &);
            virtual ~PortageEnvironment();

            ///\}

            virtual bool query_use(const UseFlagName &, const PackageDatabaseEntry &) const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual tr1::shared_ptr<const UseFlagNameCollection> known_use_expand_names(
                    const UseFlagName &, const PackageDatabaseEntry &) const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual tr1::shared_ptr<const FSEntryCollection> bashrc_files() const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual tr1::shared_ptr<const FSEntryCollection> hook_dirs() const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual const FSEntry root() const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual tr1::shared_ptr<const MirrorsCollection> mirrors(const std::string &) const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual HookResult perform_hook(const Hook &) const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual tr1::shared_ptr<PackageDatabase> package_database()
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual tr1::shared_ptr<const PackageDatabase> package_database() const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual std::string paludis_command() const
                PALUDIS_ATTRIBUTE((warn_unused_result));

            virtual void set_paludis_command(const std::string &);
    };
}

#endif
