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

#ifndef PALUDIS_GUARD_PALUDIS_PALUDIS_ENVIRONMENTS_PALUDIS_USE_CONF_HH
#define PALUDIS_GUARD_PALUDIS_PALUDIS_ENVIRONMENTS_PALUDIS_USE_CONF_HH 1

#include <paludis/util/private_implementation_pattern.hh>
#include <paludis/util/instantiation_policy.hh>
#include <paludis/config_file.hh>
#include <paludis/name.hh>

namespace paludis
{
    class PaludisEnvironment;
    class PackageDatabaseEntry;

    namespace paludis_environment
    {
        class UseConf :
            private PrivateImplementationPattern<UseConf>,
            private InstantiationPolicy<UseConf, instantiation_method::NonCopyableTag>
        {
            public:
                UseConf(const PaludisEnvironment * const);
                ~UseConf();

                void add(const FSEntry &);

                UseFlagState query(const UseFlagName &, const PackageDatabaseEntry &) const
                    PALUDIS_ATTRIBUTE((warn_unused_result));

                tr1::shared_ptr<const UseFlagNameCollection> known_use_expand_names(
                        const UseFlagName &, const PackageDatabaseEntry &) const
                    PALUDIS_ATTRIBUTE((warn_unused_result));
        };
    }
}

#endif
