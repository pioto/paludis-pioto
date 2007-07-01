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

# !!! This is used for compatibility with older Paludis versions only. When
# !!! upgrading from versions below 0.20.0, this code will be used until the
# !!! 'restart paludis' exec kicks in.

builtin_merge()
{
    ebuild_section "Merging to '${ROOT:-/}'..."

    shopt -q dotglob
    local olddotglob=$?
    shopt -s dotglob

    local v=$(vdb_path)
    if [[ -z "${v}" ]] ; then
        v=${ROOT}/var/db/pkg
    fi
    local dbdir="${v}/${CATEGORY}/${PF}"
    ebuild_section "Writing VDB entry to '${dbdir}'..."
    install -d "${dbdir}" || die "couldn't make pkg db directory (\"${dbdir}\")"
    install -d "${v}/".cache || die "couldn't make pkg db cache"

    local v VDB_FORMAT="paludis-2" COUNTER="$(date +%s )"
    for v in CATEGORY CBUILD CHOST COUNTER DEPEND DESCRIPTION EAPI \
        FEATURES HOMEPAGE INHERITED IUSE KEYWORDS LICENSE PDEPEND PF \
        PROVIDE RDEPEND SLOT SRC_URI USE CONFIG_PROTECT CONFIG_PROTECT_MASK \
        VDB_FORMAT PKGMANAGER ; do
        echo "${!v}" > "${dbdir}"/${v} || die "pkg db write ${v} failed"
        ebuild_notice "debug" "Writing VDB key ${v}=${!v}"
    done
    for v in ASFLAGS CBUILD CC CFLAGS CHOST CTARGET CXX CXXFLAGS \
        EXTRA_ECONF EXTRA_EINSTALL EXTRA_EMAKE LDFLAGS LIBCXXFLAGS \
        REPOSITORY ; do
        [[ -z "${!v}" ]] && continue
        echo "${!v}" > "${dbdir}"/${v} || die "pkg db write ${v} failed"
        ebuild_notice "debug" "Writing VDB key ${v}=${!v}"
    done

    if [[ -n ${PALUDIS_EBUILD_OVERRIDE_CONFIG_PROTECT} ]]; then
        CONFIG_PROTECT=${PALUDIS_EBUILD_OVERRIDE_CONFIG_PROTECT}
    fi
    if [[ -n ${PALUDIS_EBUILD_OVERRIDE_CONFIG_PROTECT_MASK} ]]; then
        CONFIG_PROTECT_MASK=${PALUDIS_EBUILD_OVERRIDE_CONFIG_PROTECT_MASK}
    fi

    export CONFIG_PROTECT="${CONFIG_PROTECT}"
    export CONFIG_PROTECT_MASK="${CONFIG_PROTECT_MASK}"

    [[ -f "${EBUILD}" ]] && cp "${EBUILD}" ${dbdir}/

    local reinstall=
    if [[ -f "${dbdir}/CONTENTS" ]] ; then
        mv "${dbdir}/CONTENTS" "${dbdir}/OLDCONTENTS" || die "save contents failed"
        reinstall="yes"
    fi

    ebuild_notice "debug" "Writing VDB environment.bz2"
    for v in ${USE_EXPAND} ; do
        ebuild_notice "debug" "USE_EXPAND ${v}=${!v}"
    done
    ( set ; export -p | sed 's:^declare -rx:declare -x:' ) | bzip2 > ${dbdir}/environment.bz2
    > ${dbdir}/CONTENTS

    local merge=${PALUDIS_EBUILD_DIR}/merge
    [[ -x "${merge}" ]] || merge="${PALUDIS_EBUILD_DIR_FALLBACK}"/merge
    [[ -x "${merge}" ]] || die "Couldn't find merge"

    local unmerge=${PALUDIS_EBUILD_DIR}/unmerge
    [[ -x "${unmerge}" ]] || unmerge="${PALUDIS_EBUILD_DIR_FALLBACK}"/unmerge
    [[ -x "${unmerge}" ]] || die "Couldn't find unmerge"

    if [[ -n "${D}" ]] && [[ -d "${D}" ]] ; then
        install -d "${ROOT%/}/" || die "couldn't make \${ROOT} (\"${ROOT}\")"
        if [[ -d "${D}" ]] ; then
            ${merge} "${D%/}/" "${ROOT%/}/" "${dbdir}/CONTENTS" || die "merge failed"
        fi
    fi

    echo hash -r
    hash -r

    if ! /bin/sh -c 'echo Good, our shell is still usable' ; then
        echo "Looks like our shell broke. Trying an ldconfig to fix it..."
        ldconfig -r ${ROOT}
    fi

    if [[ -n "${reinstall}" ]] ; then
        ${unmerge} "${ROOT%/}/" "${dbdir}/OLDCONTENTS" || die "unmerge failed"

        echo hash -r
        hash -r

        if ! /bin/sh -c 'echo Good, our shell is still usable' ; then
            echo "Looks like our shell broke. Trying an ldconfig to fix it..."
            ldconfig -r ${ROOT}
        fi

        rm -f "${dbdir}/OLDCONTENTS"
    fi

    [[ $olddotglob != 0 ]] && shopt -u dotglob
    shopt -q dotglob
    [[ $olddotglob == $? ]] || ebuild_notice "warning" "shopt dotglob restore failed"
}

ebuild_f_merge()
{
    local old_sandbox_write="${SANDBOX_WRITE}"
    [[ -z "${PALUDIS_DO_NOTHING_SANDBOXY}" ]] && SANDBOX_WRITE="${SANDBOX_WRITE+${SANDBOX_WRITE}:}${ROOT%/}/"
    local old_sandbox_on="${SANDBOX_ON}"
    [[ -z "${PALUDIS_DO_NOTHING_SANDBOXY}" ]] && [[ "$(canonicalise ${ROOT} )" != "/" ]] || SANDBOX_ON=0

    if hasq "merge" ${RESTRICT} ; then
        ebuild_section "Skipping builtin_merge (RESTRICT)"
    elif hasq "merge" ${SKIP_FUNCTIONS} ; then
        ebuild_section "Skipping builtin_merge (SKIP_FUNCTIONS)"
    else
        ebuild_section "Starting builtin_merge"
        builtin_merge
        ebuild_section "Done builtin_merge"
    fi

    [[ -z "${PALUDIS_DO_NOTHING_SANDBOXY}" ]] && SANDBOX_WRITE="${old_sandbox_write}"
    [[ -z "${PALUDIS_DO_NOTHING_SANDBOXY}" ]] && SANDBOX_ON="${old_sandbox_on}"
    true
}
