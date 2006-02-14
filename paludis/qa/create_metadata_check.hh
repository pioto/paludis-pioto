/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2006 Ciaran McCreesh <ciaranm@gentoo.org>
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

#ifndef PALUDIS_GUARD_PALUDIS_QA_CREATE_METADATA_CHECK_HH
#define PALUDIS_GUARD_PALUDIS_QA_CREATE_METADATA_CHECK_HH 1

#include <paludis/qa/ebuild_check.hh>
#include <string>

namespace paludis
{
    namespace qa
    {
        class CreateMetadataCheck :
            public EbuildCheck
        {
            public:
                CreateMetadataCheck();

                CheckResult operator() (const EbuildCheckData &) const;

                static const std::string & identifier();

                virtual std::string describe() const
                {
                    return "Checks that the metadata can be generated";
                }

                virtual bool is_important() const
                {
                    return true;
                }
        };

        static const EbuildCheckMaker::RegisterMaker register_create_metadata_check(
                CreateMetadataCheck::identifier(), &make_ebuild_check<CreateMetadataCheck>);
    }
}

#endif
