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

#ifndef PALUDIS_GUARD_PALUDIS_UTIL_MAKE_SHARED_PTR_HH
#define PALUDIS_GUARD_PALUDIS_UTIL_MAKE_SHARED_PTR_HH 1

#include <paludis/util/tr1_memory.hh>
#include <paludis/util/make_shared_ptr-fwd.hh>

namespace paludis
{
    template <typename T_>
    tr1::shared_ptr<T_>
    make_shared_ptr(T_ * const t)
    {
        return tr1::shared_ptr<T_>(t);
    }
}

#endif