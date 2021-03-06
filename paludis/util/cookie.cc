/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2008 Ciaran McCreesh
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

#include <paludis/util/cookie.hh>
#include <paludis/util/stringify.hh>
#include <sys/types.h>
#include <sys/time.h>

using namespace paludis;

std::string
paludis::cookie()
{
    struct timeval t;
    gettimeofday(&t, 0);
    return "C." + stringify(::getpid()) + "." + stringify(t.tv_sec) + "." + stringify(t.tv_usec) + ".C";
}

