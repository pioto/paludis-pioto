#!/bin/bash
# vim: set sw=4 sts=4 et :

# Copyright (c) 2006, 2007 Ciaran McCreesh <ciaranm@ciaranm.org>
#
# This file is part of the Paludis package manager. Paludis is free software;
# you can redistribute it and/or modify it under the terms of the GNU General
# Public License, version 2, as published by the Free Software Foundation.
#
# Paludis is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA  02111-1307  USA

builtin_unpackbin()
{
    [[ -n "${B}" ]] && unpack --binary ${B}
    rm -f ${D}/.paludis-binpkg-environment
}

ebuild_f_unpackbin()
{
    cd ${WORKDIR} || die "cd to \${WORKDIR} (\"${WORKDIR}\") failed"

    if hasq "unpackbin" ${RESTRICT} ; then
        ebuild_section "Skipping builtin_unpackbin (RESTRICT)"
    elif hasq "unpackbin" ${SKIP_FUNCTIONS} ; then
        ebuild_section "Skipping builtin_unpackbin (SKIP_FUNCTIONS)"
    else
        ebuild_section "Starting builtin_unpackbin"
        builtin_unpackbin
        ebuild_section "Done builtin_unpackbin"
    fi
}

