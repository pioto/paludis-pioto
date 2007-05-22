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

#ifndef PALUDIS_GUARD_PALUDIS_UTIL_TR1_TYPE_TRAITS_HH
#define PALUDIS_GUARD_PALUDIS_UTIL_TR1_TYPE_TRAITS_HH 1

#if defined(PALUDIS_TR1_TYPE_TRAITS_IS_STD_TR1)

#include <tr1/type_traits>

namespace paludis
{
    namespace tr1
    {
        using std::tr1::remove_pointer;
    }
}

#elif defined(PALUDIS_TR1_TYPE_TRAITS_IS_BOOST)

#include <boost/type_traits.hpp>

namespace paludis
{
    namespace tr1
    {
        using boost::remove_pointer;
    }
}

#else
#  error Either PALUDIS_TR1_TYPE_TRAITS_IS_STD_TR1 or PALUDIS_TR1_TYPE_TRAITS_IS_BOOST should be defined
#endif

#endif