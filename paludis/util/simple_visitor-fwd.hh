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

#ifndef PALUDIS_GUARD_PALUDIS_UTIL_SIMPLE_VISITOR_FWD_HH
#define PALUDIS_GUARD_PALUDIS_UTIL_SIMPLE_VISITOR_FWD_HH 1

#include <paludis/util/type_list-fwd.hh>

namespace paludis
{
    template <typename TypeList_>
    struct DeclareAbstractVisitMethods;

    template <>
    struct DeclareAbstractVisitMethods<TypeListTail>;

    template <typename TypeList_>
    struct WrappedVisitorBase;

    template <typename RealClass_, typename TypeList_>
    struct ImplementVisitMethods;

    template <typename RealClass_>
    struct ImplementVisitMethods<RealClass_, TypeListTail>;

    template <typename TypeList_, typename UnwrappedVisitor_>
    struct WrappedVoidResultVisitor;

    template <typename TypeList_, typename Result_, typename UnwrappedVisitor_>
    struct WrappedNonVoidResultVisitor;

    template <typename BaseClass_, typename VisitableTypeList_>
    struct DeclareAbstractAcceptMethods;

    template <typename BaseClass_, typename RealClass_>
    struct ImplementAcceptMethods;
}

#endif
