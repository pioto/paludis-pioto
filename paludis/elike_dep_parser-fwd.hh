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

#ifndef PALUDIS_GUARD_PALUDIS_ELIKE_DEP_PARSER_FWD_HH
#define PALUDIS_GUARD_PALUDIS_ELIKE_DEP_PARSER_FWD_HH 1

#include <paludis/util/kc-fwd.hh>
#include <paludis/util/keys.hh>
#include <paludis/util/attributes.hh>
#include <paludis/util/map-fwd.hh>
#include <paludis/name-fwd.hh>
#include <tr1/functional>
#include <tr1/memory>

namespace paludis
{
    typedef std::tr1::function<void (const std::string &)> ELikeDepParserStringFunction;
    typedef std::tr1::function<void (const std::string &, const std::string &)> ELikeDepParserArrowFunction;
    typedef std::tr1::function<void ()> ELikeDepParserAnyFunction;
    typedef std::tr1::function<void ()> ELikeDepParserAllFunction;
    typedef std::tr1::function<void (const std::string &)> ELikeDepParserUseFunction;
    typedef std::tr1::function<void (const std::string &)> ELikeDepParserLabelFunction;
    typedef std::tr1::function<void ()> ELikeDepParserPushFunction;
    typedef std::tr1::function<void ()> ELikeDepParserPopFunction;
    typedef std::tr1::function<void ()> ELikeDepParserShouldBeEmptyFunction;
    typedef std::tr1::function<void (const std::string &, const std::string::size_type &, const std::string &)> ELikeDepParserErrorFunction;
    typedef std::tr1::function<void ()> ELikeDepParserUseUnderAnyFunction;
    typedef std::tr1::function<void (const std::tr1::shared_ptr<const Map<std::string, std::string> > &)>
        ELikeDepParserAnnotationsFunction;

    typedef kc::KeyedClass<
        kc::Field<k::on_string, ELikeDepParserStringFunction>,
        kc::Field<k::on_arrow, ELikeDepParserArrowFunction>,
        kc::Field<k::on_any, ELikeDepParserAnyFunction>,
        kc::Field<k::on_all, ELikeDepParserAllFunction>,
        kc::Field<k::on_use, ELikeDepParserUseFunction>,
        kc::Field<k::on_label, ELikeDepParserLabelFunction>,
        kc::Field<k::on_pop, ELikeDepParserPopFunction>,
        kc::Field<k::on_error, ELikeDepParserErrorFunction>,
        kc::Field<k::on_should_be_empty, ELikeDepParserShouldBeEmptyFunction>,
        kc::Field<k::on_use_under_any, ELikeDepParserUseUnderAnyFunction>,
        kc::Field<k::on_annotations, ELikeDepParserAnnotationsFunction>
            > ELikeDepParserCallbacks;

    void parse_elike_dependencies(const std::string &, const ELikeDepParserCallbacks & callbacks) PALUDIS_VISIBLE;
}

#endif
