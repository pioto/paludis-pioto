/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2006, 2007, 2008 Ciaran McCreesh
 * Copyright (c) 2009 David Leverton
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

#include "pipe.hh"
#include <paludis/util/exception.hh>
#include <paludis/util/log.hh>
#include <unistd.h>

using namespace paludis;

Pipe::Pipe()
{
    Context context("When creating pipe FDs:");

    if (-1 == pipe(_fds))
        throw InternalError(PALUDIS_HERE, "pipe(2) failed");

    Log::get_instance()->message("util.pipe.fds", ll_debug, lc_context) << "Pipe FDs are '" << read_fd() << "', '"
        << write_fd() << "'";
}

Pipe::~Pipe()
{
}

