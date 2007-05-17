/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2005, 2006, 2007 Ciaran McCreesh <ciaranm@ciaranm.org>
 * Copyright (c) 2006 Danny van Dyk <kugelfang@gentoo.org>
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

#include <paludis/repositories/gentoo/layout.hh>
#include <paludis/repositories/gentoo/traditional_layout.hh>
#include <paludis/util/collection_concrete.hh>
#include <paludis/util/fs_entry.hh>
#include <paludis/util/virtual_constructor-impl.hh>

using namespace paludis;

template class VirtualConstructor<std::string,
         std::tr1::shared_ptr<Layout> (*) (const RepositoryName &, const FSEntry &,
                 std::tr1::shared_ptr<const PortageRepositoryEntries>),
         virtual_constructor_not_found::ThrowException<NoSuchLayoutType> >;

Layout::Layout() :
    _profiles_dirs(new FSEntryCollection::Concrete)
{
}

Layout::~Layout()
{
}

Layout::ProfilesDirsIterator
Layout::begin_profiles_dirs() const
{
    return ProfilesDirsIterator(_profiles_dirs->begin());
}

Layout::ProfilesDirsIterator
Layout::end_profiles_dirs() const
{
    return ProfilesDirsIterator(_profiles_dirs->end());
}

void
Layout::add_profiles_dir(const FSEntry & f)
{
    _profiles_dirs->push_back(f);
}

namespace
{
    template <typename T_>
    std::tr1::shared_ptr<Layout>
    make_layout(const RepositoryName & n, const FSEntry & b,
            std::tr1::shared_ptr<const PortageRepositoryEntries> e)
    {
        return std::tr1::shared_ptr<Layout>(new T_(n, b, e));
    }
}

LayoutMaker::LayoutMaker()
{
    register_maker("traditional", &make_layout<TraditionalLayout>);
}

NoSuchLayoutType::NoSuchLayoutType(const std::string & format) throw () :
    ConfigurationError("No available maker for Portage repository layout type '" + format + "'")
{
}
