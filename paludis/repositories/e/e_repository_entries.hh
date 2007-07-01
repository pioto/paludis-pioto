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

#ifndef PALUDIS_GUARD_PALUDIS_REPOSITORIES_E_E_REPOSITORY_METADATA_HH
#define PALUDIS_GUARD_PALUDIS_REPOSITORIES_E_E_REPOSITORY_METADATA_HH 1

#include <paludis/name-fwd.hh>
#include <paludis/repository-fwd.hh>
#include <paludis/version_spec-fwd.hh>
#include <paludis/package_id-fwd.hh>
#include <paludis/util/instantiation_policy.hh>
#include <paludis/util/virtual_constructor.hh>
#include <paludis/repositories/e/e_repository_profile.hh>
#include <paludis/repositories/e/e_repository_params.hh>
#include <string>

/** \file
 * Declaration for the ERepositoryEntries class.
 *
 * \ingroup grperepository
 */

namespace paludis
{
    class ERepository;
    class Environment;

    /**
     * Handle entries (for example, ebuilds) in a ERepository.
     *
     * \ingroup grperepository
     * \nosubgrouping
     */
    class PALUDIS_VISIBLE ERepositoryEntries
    {
        public:
            ///\name Basic operations
            ///\{

            virtual ~ERepositoryEntries() = 0;

            ///\}

            virtual bool is_package_file(const QualifiedPackageName &, const FSEntry &) const
                PALUDIS_ATTRIBUTE((warn_unused_result)) = 0;

            virtual VersionSpec extract_package_file_version(const QualifiedPackageName &, const FSEntry &) const
                PALUDIS_ATTRIBUTE((warn_unused_result)) = 0;

            /**
             * Create a PackageID.
             */
            virtual const tr1::shared_ptr<const PackageID> make_id(const QualifiedPackageName &, const VersionSpec &,
                    const FSEntry &, const std::string &) const
                PALUDIS_ATTRIBUTE((warn_unused_result)) = 0;

            /**
             * Fetch an environment variable.
             */
            virtual std::string get_environment_variable(const tr1::shared_ptr<const PackageID> &, const std::string & var,
                    tr1::shared_ptr<const ERepositoryProfile>) const = 0;

            /**
             * Handle an install.
             */
            virtual void install(const tr1::shared_ptr<const PackageID> &, const InstallOptions &,
                    tr1::shared_ptr<const ERepositoryProfile>) const = 0;

            /**
             * Handle a pretend.
             */
            virtual bool pretend(const tr1::shared_ptr<const PackageID> &,
                    tr1::shared_ptr<const ERepositoryProfile>) const = 0;

            /**
             * Handle a merge.
             */
            virtual void merge(const MergeOptions &) = 0;
    };

    /**
     * Thrown if a repository of the specified type does not exist.
     *
     * \ingroup grpexceptions
     * \ingroup grprepository
     * \nosubgrouping
     */
    class PALUDIS_VISIBLE NoSuchERepositoryEntriesType : public ConfigurationError
    {
        public:
            /**
             * Constructor.
             */
            NoSuchERepositoryEntriesType(const std::string & format) throw ();
    };

    /**
     * Virtual constructor for ERepositoryEntries.
     *
     * \ingroup grprepository
     */
    class PALUDIS_VISIBLE ERepositoryEntriesMaker :
        public VirtualConstructor<std::string,
            tr1::shared_ptr<ERepositoryEntries> (*) (const Environment * const, ERepository * const,
                    const ERepositoryParams &),
            virtual_constructor_not_found::ThrowException<NoSuchERepositoryEntriesType> >,
        public InstantiationPolicy<ERepositoryEntriesMaker, instantiation_method::SingletonTag>
    {
        friend class InstantiationPolicy<ERepositoryEntriesMaker, instantiation_method::SingletonTag>;

        private:
            ERepositoryEntriesMaker();
    };

}

#endif