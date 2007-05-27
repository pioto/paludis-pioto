/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2007 David Leverton <u01drl3@abdn.ac.uk>
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

#ifndef PALUDIS_GUARD_PALUDIS_UTIL_CLONE_HH
#define PALUDIS_GUARD_PALUDIS_UTIL_CLONE_HH 1

#include <paludis/util/attributes.hh>
#include <paludis/util/tr1_memory.hh>

/** \file
 * Declares the Cloneable class and helpers.
 *
 * \ingroup grpclone
 */

namespace paludis
{
    /**
     * Base class for objects that can be cloned.
     *
     * \ingroup grpclone
     * \nosubgrouping
     */
    template <typename T_>
    class PALUDIS_VISIBLE Cloneable
    {
        public:
            ///\name Cloning
            ///\{

            /**
             * Return a new copy of ourselves.
             */
            virtual tr1::shared_ptr<T_> clone() const PALUDIS_ATTRIBUTE((warn_unused_result)) = 0;

            ///\}

            ///\name Basic operations
            ///\{

            virtual ~Cloneable();

            ///\}
    };

    /**
     * Helper class implementing the clone() method using the copy
     * contructor.
     *
     * \ingroup grpclone
     * \nosubgrouping
     */
    template <typename Base_, typename Child_>
    class PALUDIS_VISIBLE CloneUsingThis :
        public virtual Cloneable<Base_>
    {
        public:
            virtual tr1::shared_ptr<Base_> clone() const PALUDIS_ATTRIBUTE((warn_unused_result));

            ///\name Basic operations
            ///\{

            virtual ~CloneUsingThis();

            ///\}
    };
}

#endif

