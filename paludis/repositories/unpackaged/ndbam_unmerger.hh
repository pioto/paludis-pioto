/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2007 Ciaran McCreesh <ciaranm@ciaranm.org>
 * Copyright (c) 2007 Piotr Jaroszyński <peper@gentoo.org>
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

#ifndef PALUDIS_GUARD_PALUDIS_REPOSITORIES_UNPACKAGED_NDBAM_UNMERGER_HH
#define PALUDIS_GUARD_PALUDIS_REPOSITORIES_UNPACKAGED_NDBAM_UNMERGER_HH 1

#include <paludis/util/private_implementation_pattern.hh>
#include <paludis/util/sr.hh>
#include <paludis/util/fs_entry.hh>
#include <paludis/unmerger.hh>
#include <paludis/environment-fwd.hh>
#include <paludis/package_id-fwd.hh>

namespace paludis
{

    namespace unpackaged_repositories
    {
        class NDBAM;

#include <paludis/repositories/unpackaged/ndbam_unmerger-sr.hh>

        class PALUDIS_VISIBLE NDBAMUnmergerError :
            public UnmergerError
        {
            public:
                NDBAMUnmergerError(const std::string &) throw ();
        };

        class PALUDIS_VISIBLE NDBAMUnmerger :
            public Unmerger,
            private PrivateImplementationPattern<NDBAMUnmerger>
        {
            private:
                Implementation<NDBAMUnmerger> * _imp;
                class FileExtraInfo;
                class SymlinkExtraInfo;

                void _add_file(const FSEntry & f, const std::string & md5, const time_t mtime);
                void _add_dir(const FSEntry & f);
                void _add_sym(const FSEntry & f, const std::string & target, const time_t mtime);

            protected:
                bool config_protected(const FSEntry &) const;
                std::string make_tidy(const FSEntry &) const;

                void populate_unmerge_set();

                void display(const std::string &) const;

                bool check_file(const FSEntry &, tr1::shared_ptr<ExtraInfo>) const;
                bool check_dir(const FSEntry &, tr1::shared_ptr<ExtraInfo>) const;
                bool check_sym(const FSEntry &, tr1::shared_ptr<ExtraInfo>) const;
                bool check_misc(const FSEntry &, tr1::shared_ptr<ExtraInfo>) const;

            public:
                NDBAMUnmerger(const NDBAMUnmergerOptions &);
                ~NDBAMUnmerger();

                virtual Hook extend_hook(const Hook &) const;
        };
    }

}


#endif