/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2007 David Leverton <levertond@googlemail.com>
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

#ifndef PALUDIS_GUARD_RECONCILIO_BROKEN_LINKAGE_FINDER_CONFIGURATION_HH
#define PALUDIS_GUARD_RECONCILIO_BROKEN_LINKAGE_FINDER_CONFIGURATION_HH

#include <paludis/util/attributes.hh>
#include <paludis/util/fs_entry-fwd.hh>
#include <paludis/util/instantiation_policy.hh>
#include <paludis/util/private_implementation_pattern.hh>

#include <libwrapiter/libwrapiter_forward_iterator-fwd.hh>

#include <string>

namespace broken_linkage_finder
{
    class Configuration :
        private paludis::PrivateImplementationPattern<Configuration>,
        private paludis::InstantiationPolicy<Configuration, paludis::instantiation_method::NonCopyableTag>
    {
        public:
            Configuration(const paludis::FSEntry &);
            ~Configuration();

            typedef libwrapiter::ForwardIterator<Configuration, const paludis::FSEntry> DirsIterator;
            DirsIterator begin_search_dirs() const PALUDIS_ATTRIBUTE((warn_unused_result));
            DirsIterator end_search_dirs() const PALUDIS_ATTRIBUTE((warn_unused_result));

            bool dir_is_masked(const paludis::FSEntry &) const PALUDIS_ATTRIBUTE((warn_unused_result));
            bool lib_is_masked(const std::string &) const PALUDIS_ATTRIBUTE((warn_unused_result));
    };
}

#endif

