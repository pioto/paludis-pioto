/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2005, 2006, 2007 Ciaran McCreesh <ciaranm@ciaranm.org>
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

#include <paludis/dep_spec.hh>
#include <paludis/repositories/e/eapi.hh>
#include <paludis/repositories/e/dep_lexer.hh>
#include <paludis/repositories/e/dep_parser.hh>
#include <paludis/util/exception.hh>
#include <paludis/util/stringify.hh>
#include <paludis/util/tr1_functional.hh>
#include <paludis/util/visitor-impl.hh>
#include <paludis/util/make_shared_ptr.hh>
#include <libwrapiter/libwrapiter_forward_iterator.hh>
#include <libwrapiter/libwrapiter_output_iterator.hh>
#include <stack>

/** \file
 * Implementation for dep_parser.hh things.
 *
 * \ingroup grpdepparser
 */

using namespace paludis;
using namespace paludis::erepository;

#include <paludis/repositories/e/dep_parser-se.cc>

DepStringParseError::DepStringParseError(const std::string & d,
        const std::string & m) throw () :
    DepStringError(d, "in parse phase: " + m)
{
}

DepStringNestingError::DepStringNestingError(const std::string & dep_string) throw () :
    DepStringParseError(dep_string, "improperly balanced parentheses")
{
}

namespace
{
    struct LabelsAreURI;

    enum DepParserState
    {
        dps_initial,
        dps_had_double_bar,
        dps_had_double_bar_space,
        dps_had_paren,
        dps_had_use_flag,
        dps_had_use_flag_space,
        dps_had_text_arrow,
        dps_had_text_arrow_space,
        dps_had_text_arrow_text,
        dps_had_label
    };

    using namespace tr1::placeholders;

    struct ParsePackageDepSpec
    {
        PackageDepSpecParseMode _parse_mode;

        ParsePackageDepSpec(PackageDepSpecParseMode m) :
            _parse_mode(m)
        {
        }

        template <typename H_>
        void
        add(const std::string & s, tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)> & p) const
        {
            p(tr1::shared_ptr<TreeLeaf<H_, PackageDepSpec> >(new TreeLeaf<H_, PackageDepSpec>(
                            tr1::shared_ptr<PackageDepSpec>(new PackageDepSpec(s, _parse_mode)))));
        }

        template <typename H_>
        void
        add_arrow(const std::string & lhs, const std::string & rhs, tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)> &) const
        {
            throw DepStringParseError(lhs + " -> " + rhs, "Arrows not allowed in this context");
        }
    };

    struct ParsePackageOrBlockDepSpec
    {
        PackageDepSpecParseMode _parse_mode;

        ParsePackageOrBlockDepSpec(PackageDepSpecParseMode m) :
            _parse_mode(m)
        {
        }

        template <typename H_>
        void
        add(const std::string & s, tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)> & p) const
        {
            if (s.empty() || '!' != s.at(0))
                p(tr1::shared_ptr<TreeLeaf<H_, PackageDepSpec> >(new TreeLeaf<H_, PackageDepSpec>(
                                tr1::shared_ptr<PackageDepSpec>(new PackageDepSpec(s, _parse_mode)))));
            else
                p(tr1::shared_ptr<TreeLeaf<H_, BlockDepSpec> >(new TreeLeaf<H_, BlockDepSpec>(
                                tr1::shared_ptr<BlockDepSpec>(new BlockDepSpec(
                                        tr1::shared_ptr<PackageDepSpec>(new PackageDepSpec(s.substr(1), _parse_mode)))))));
        }

        template <typename H_>
        void
        add_arrow(const std::string & lhs, const std::string & rhs, tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)> &) const
        {
            throw DepStringParseError(lhs + " -> " + rhs, "Arrows not allowed in this context");
        }
    };

    struct ParseTextDepSpec
    {
        template <typename H_>
        void
        add(const std::string & s, tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)> & p) const
        {
            p(tr1::shared_ptr<TreeLeaf<H_, PlainTextDepSpec> >(new TreeLeaf<H_, PlainTextDepSpec>(
                            tr1::shared_ptr<PlainTextDepSpec>(new PlainTextDepSpec(s)))));
        }

        template <typename H_>
        void
        add_arrow(const std::string & lhs, const std::string & rhs, tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)> &) const
        {
            throw DepStringParseError(lhs + " -> " + rhs, "Arrows not allowed in this context");
        }
    };

    struct ParseURIDepSpec
    {
        const bool _supports_arrow;

        ParseURIDepSpec(bool a) :
            _supports_arrow(a)
        {
        }

        template <typename H_>
        void
        add(const std::string & s, tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)> & p) const
        {
            p(tr1::shared_ptr<TreeLeaf<H_, URIDepSpec> >(new TreeLeaf<H_, URIDepSpec>(
                            tr1::shared_ptr<URIDepSpec>(new URIDepSpec(s)))));
        }

        template <typename H_>
        void
        add_arrow(const std::string & lhs, const std::string & rhs, tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)> & p) const
        {
            if (_supports_arrow)
                p(tr1::shared_ptr<TreeLeaf<H_, URIDepSpec> >(new TreeLeaf<H_, URIDepSpec>(
                                tr1::shared_ptr<URIDepSpec>(new URIDepSpec(lhs + " -> " + rhs)))));
            else
                throw DepStringParseError(lhs + " -> " + rhs, "Arrows not allowed in this EAPI");
        }
    };

    template <typename H_, bool>
    struct HandleUse
    {
        static void handle(const std::string & s, const std::string & i,
                std::stack<std::pair<tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)>, bool> > & stack)
        {
            std::string f(i);
            bool inv(f.length() && ('!' == f.at(0)));
            if (inv)
                f.erase(0, 1);

            if (f.empty())
                throw DepStringParseError(s,
                        "Bad use flag name '" + i + "'");
            if ('?' != f.at(f.length() - 1))
                throw DepStringParseError(s,
                        "Use flag name '" + i + "' missing '?'");

            f.erase(f.length() - 1);
            tr1::shared_ptr<ConstTreeSequence<H_, UseDepSpec> > a(
                    new ConstTreeSequence<H_, UseDepSpec>(tr1::shared_ptr<UseDepSpec>(
                            new UseDepSpec(UseFlagName(f), inv))));
            stack.top().first(a);
            stack.push(std::make_pair(tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)>(
                    tr1::bind(&ConstTreeSequence<H_, UseDepSpec>::add, a, _1)), false));
        }
    };

    template <typename H_>
    struct HandleUse<H_, false>
    {
        static void handle(const std::string & s, const std::string &,
                std::stack<std::pair<tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)>, bool> > &)
        {
            throw DepStringParseError(s, "use? group is not allowed here");
        }
    };

    template <typename H_, bool>
    struct HandleAny
    {
        static void handle(const std::string &, std::stack<std::pair<tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)>, bool> > &
                stack)
        {
             tr1::shared_ptr<ConstTreeSequence<H_, AnyDepSpec> > a(new ConstTreeSequence<H_, AnyDepSpec>(
                         tr1::shared_ptr<AnyDepSpec>(new AnyDepSpec)));
             stack.top().first(a);
             stack.push(std::make_pair(tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)>(
                     tr1::bind(&ConstTreeSequence<H_, AnyDepSpec>::add, a, _1)), true));
        }
    };

    template <typename H_>
    struct HandleAny<H_, false>
    {
        static void handle(const std::string & s, std::stack<std::pair<tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)>, bool> > &)
        {
             throw DepStringParseError(s, "|| is not allowed here");
        }
    };

    template <typename H_, typename K_>
    struct HandleLabel
    {
        static void add(const std::string & s, tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)> &,
                const EAPI &)
        {
            throw DepStringParseError(s, "label is not allowed here");
        }
    };

    template <typename H_>
    struct HandleLabel<H_, LabelsAreURI>
    {
        static void add(const std::string & s, tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)> & p,
                const EAPI & e)
        {
            if (e.supported && e.supported->uri_labels)
                p(tr1::shared_ptr<TreeLeaf<H_, LabelsDepSpec<URILabelVisitorTypes> > >(
                            new TreeLeaf<H_, LabelsDepSpec<URILabelVisitorTypes> >(parse_uri_label(s, e))));
            else
                throw DepStringParseError(s, "URI labels not allowed in this EAPI");
        }
    };

    bool disallow_any_use(const DependencySpecTreeParseMode tree_mode)
    {
        switch (tree_mode)
        {
            case dst_pm_eapi_0:
                return false;

            case dst_pm_paludis_1:
            case dst_pm_exheres_0:
                return true;

            case last_dst_pm:
                ;
        }

        throw InternalError(PALUDIS_HERE, "bad _tree_mode");
    }
}

namespace
{
    template <typename H_, typename I_, bool any_, bool use_, typename Label_>
    tr1::shared_ptr<typename H_::ConstItem>
    parse(const std::string & s, bool disallow_any_use, const I_ & p, const EAPI & e)
    {
        Context context("When parsing dependency string '" + s + "':");

        tr1::shared_ptr<ConstTreeSequence<H_, AllDepSpec> > result(
            new ConstTreeSequence<H_, AllDepSpec>(tr1::shared_ptr<AllDepSpec>(new AllDepSpec)));
        std::stack<std::pair<tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)>, bool> > stack;
        stack.push(std::make_pair(tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)>(
                tr1::bind(&ConstTreeSequence<H_, AllDepSpec>::add, result, _1)), false));

        std::string arrow_lhs;
        DepParserState state(dps_initial);
        DepLexer lexer(s);
        DepLexer::Iterator i(lexer.begin()), i_end(lexer.end());

        for ( ; i != i_end ; ++i)
        {
            Context local_context("When handling lexer token '" + i->second +
                    "' (" + stringify(i->first) + "):");
            do
            {
                switch (state)
                {
                    case dps_initial:
                        do
                        {
                            switch (i->first)
                            {
                                case dpl_whitespace:
                                     continue;

                                case dpl_arrow:
                                     throw DepStringParseError(s, "Arrow not allowed here");

                                case dpl_text:
                                     {
                                         if (i->second.empty())
                                             throw DepStringParseError(i->second, "Empty text entry");

                                         DepLexer::Iterator i_fwd(next(i));
                                         if (i_fwd != i_end && i_fwd->first == dpl_whitespace && ++i_fwd != i_end
                                                 && i_fwd->first == dpl_arrow)
                                         {
                                             arrow_lhs = i->second;
                                             i = i_fwd;
                                             state = dps_had_text_arrow;
                                         }
                                         else
                                             p.template add<H_>(i->second, stack.top().first);
                                     }
                                     continue;

                                case dpl_open_paren:
                                     {
                                         tr1::shared_ptr<ConstTreeSequence<H_, AllDepSpec> > a(new ConstTreeSequence<H_, AllDepSpec>(
                                                     tr1::shared_ptr<AllDepSpec>(new AllDepSpec)));
                                         stack.top().first(a);
                                         stack.push(std::make_pair(tr1::function<void (tr1::shared_ptr<ConstAcceptInterface<H_> >)>(
                                                         tr1::bind(&ConstTreeSequence<H_, AllDepSpec>::add, a, _1)), false));
                                         state = dps_had_paren;
                                     }
                                     continue;

                                case dpl_close_paren:
                                     if (stack.empty())
                                         throw DepStringNestingError(s);
                                     stack.pop();
                                     if (stack.empty())
                                         throw DepStringNestingError(s);
                                     state = dps_had_paren;
                                     continue;

                                case dpl_double_bar:
                                     HandleAny<H_, any_>::handle(s, stack);
                                     state = dps_had_double_bar;
                                     continue;

                                case dpl_use_flag:
                                     if (use_ && disallow_any_use && stack.top().second)
                                         throw DepStringParseError(s, "use? group is not allowed immediately under a || ( )");
                                     HandleUse<H_, use_>::handle(s, i->second, stack);
                                     state = dps_had_use_flag;
                                     continue;

                                case dpl_label:
                                     HandleLabel<H_, Label_>::add(i->second, stack.top().first, e);
                                     state = dps_had_label;
                                     continue;
                            }
                            throw InternalError(PALUDIS_HERE,
                                    "dps_initial: i->first is " + stringify(i->first));

                        } while (0);
                        continue;

                    case dps_had_double_bar:
                        do
                        {
                            switch (i->first)
                            {
                                case dpl_whitespace:
                                    state = dps_had_double_bar_space;
                                    continue;

                                case dpl_text:
                                case dpl_arrow:
                                case dpl_use_flag:
                                case dpl_double_bar:
                                case dpl_open_paren:
                                case dpl_close_paren:
                                case dpl_label:
                                    throw DepStringParseError(s, "Expected space after '||'");
                            }
                            throw InternalError(PALUDIS_HERE,
                                    "dps_had_double_bar: i->first is " + stringify(i->first));

                        } while (0);
                        continue;

                    case dps_had_double_bar_space:
                        do
                        {
                            switch (i->first)
                            {
                                case dpl_open_paren:
                                    state = dps_initial;
                                    continue;

                                case dpl_whitespace:
                                case dpl_text:
                                case dpl_use_flag:
                                case dpl_double_bar:
                                case dpl_close_paren:
                                case dpl_arrow:
                                case dpl_label:
                                    throw DepStringParseError(s, "Expected '(' after '|| '");
                            }
                            throw InternalError(PALUDIS_HERE,
                                    "dps_had_double_bar_space: i->first is " + stringify(i->first));
                        } while (0);
                        continue;

                    case dps_had_paren:
                        do
                        {
                            switch (i->first)
                            {
                                case dpl_whitespace:
                                    state = dps_initial;
                                    continue;

                                case dpl_text:
                                case dpl_use_flag:
                                case dpl_double_bar:
                                case dpl_open_paren:
                                case dpl_close_paren:
                                case dpl_arrow:
                                case dpl_label:
                                    throw DepStringParseError(s, "Expected space after '(' or ')'");
                            }
                            throw InternalError(PALUDIS_HERE,
                                    "dps_had_paren: i->first is " + stringify(i->first));
                        } while (0);
                        continue;

                    case dps_had_use_flag:
                        do
                        {
                            switch (i->first)
                            {
                                case dpl_whitespace:
                                    state = dps_had_use_flag_space;
                                    continue;

                                case dpl_text:
                                case dpl_use_flag:
                                case dpl_double_bar:
                                case dpl_open_paren:
                                case dpl_close_paren:
                                case dpl_arrow:
                                case dpl_label:
                                    throw DepStringParseError(s, "Expected space after use flag");
                            }
                            throw InternalError(PALUDIS_HERE,
                                    "dps_had_use_flag: i->first is " + stringify(i->first));
                        } while (0);
                        continue;

                    case dps_had_use_flag_space:
                        do
                        {
                            switch (i->first)
                            {
                                case dpl_open_paren:
                                    state = dps_had_paren;
                                    continue;

                                case dpl_whitespace:
                                case dpl_text:
                                case dpl_use_flag:
                                case dpl_double_bar:
                                case dpl_close_paren:
                                case dpl_arrow:
                                case dpl_label:
                                    throw DepStringParseError(s, "Expected '(' after use flag");
                            }
                            throw InternalError(PALUDIS_HERE,
                                    "dps_had_use_flag_space: i->first is " + stringify(i->first));
                        } while (0);
                        continue;

                    case dps_had_label:
                        do
                        {
                            switch (i->first)
                            {
                                case dpl_whitespace:
                                    state = dps_initial;
                                    continue;

                                case dpl_text:
                                case dpl_use_flag:
                                case dpl_double_bar:
                                case dpl_open_paren:
                                case dpl_close_paren:
                                case dpl_arrow:
                                case dpl_label:
                                    throw DepStringParseError(s, "Expected space after label");
                            }
                            throw InternalError(PALUDIS_HERE,
                                    "dps_had_label: i->first is " + stringify(i->first));
                        } while (0);
                        continue;

                    case dps_had_text_arrow:
                        do
                        {
                            switch (i->first)
                            {
                                case dpl_whitespace:
                                    state = dps_had_text_arrow_space;
                                    continue;

                                case dpl_text:
                                case dpl_open_paren:
                                case dpl_use_flag:
                                case dpl_double_bar:
                                case dpl_close_paren:
                                case dpl_arrow:
                                case dpl_label:
                                    throw DepStringParseError(s, "Expected whitespace after arrow");
                            }
                            throw InternalError(PALUDIS_HERE,
                                    "dps_had_text_arrow: i->first is " + stringify(i->first));
                        } while (0);
                        continue;

                    case dps_had_text_arrow_space:
                        do
                        {
                            switch (i->first)
                            {
                                case dpl_whitespace:
                                    continue;

                                case dpl_text:
                                    state = dps_had_text_arrow_text;
                                    p.template add_arrow<H_>(arrow_lhs, i->second, stack.top().first);
                                    continue;

                                case dpl_open_paren:
                                case dpl_use_flag:
                                case dpl_double_bar:
                                case dpl_close_paren:
                                case dpl_arrow:
                                case dpl_label:
                                    throw DepStringParseError(s, "Expected text after whitespace after arrow");
                            }
                            throw InternalError(PALUDIS_HERE,
                                    "dps_had_text_arrow_space: i->first is " + stringify(i->first));
                        } while (0);
                        continue;

                    case dps_had_text_arrow_text:
                        do
                        {
                            switch (i->first)
                            {
                                case dpl_whitespace:
                                    state = dps_initial;
                                    continue;

                                case dpl_text:
                                case dpl_open_paren:
                                case dpl_use_flag:
                                case dpl_close_paren:
                                case dpl_double_bar:
                                case dpl_arrow:
                                case dpl_label:
                                    throw DepStringParseError(s, "Expected whitespace after text after whitespace after arrow");
                            }
                            throw InternalError(PALUDIS_HERE,
                                    "dps_had_text_arrow_text: i->first is " + stringify(i->first));
                        }
                        while (0);
                        continue;
                }
                throw InternalError(PALUDIS_HERE,
                        "state is " + stringify(state));

            } while (0);
        }

        if (stack.empty())
            throw DepStringNestingError(s);

        switch (state)
        {
            case dps_initial:
            case dps_had_paren:
            case dps_had_text_arrow_text:
            case dps_had_text_arrow_space:
                break;

            case dps_had_double_bar_space:
            case dps_had_double_bar:
            case dps_had_use_flag:
            case dps_had_use_flag_space:
            case dps_had_text_arrow:
            case dps_had_label:
                throw DepStringParseError(s, "Unexpected end of string");
        }

        stack.pop();
        if (! stack.empty())
            throw DepStringNestingError(s);
        return result;
    }
}

tr1::shared_ptr<DependencySpecTree::ConstItem>
paludis::erepository::parse_depend(const std::string & s, const EAPI & e)
{
    Context c("When parsing dependency string '" + s + "' using EAPI '" + e.name + "':");

    if (! e.supported)
        throw DepStringParseError(s, "Don't know how to parse EAPI '" + e.name + "' dependencies");

    return parse<DependencySpecTree, ParsePackageOrBlockDepSpec, true, true, void>(s,
            disallow_any_use(e.supported->dependency_spec_tree_parse_mode),
            ParsePackageOrBlockDepSpec(e.supported->package_dep_spec_parse_mode), e);
}

tr1::shared_ptr<ProvideSpecTree::ConstItem>
paludis::erepository::parse_provide(const std::string & s, const EAPI & e)
{
    Context c("When parsing provide string '" + s + "' using EAPI '" + e.name + "':");

    if (! e.supported)
        throw DepStringParseError(s, "Don't know how to parse EAPI '" + e.name + "' provides");

    return parse<ProvideSpecTree, ParsePackageDepSpec, false, true, void>(s, false,
            ParsePackageDepSpec(pds_pm_eapi_0), e);
}

tr1::shared_ptr<RestrictSpecTree::ConstItem>
paludis::erepository::parse_restrict(const std::string & s, const EAPI & e)
{
    Context c("When parsing restrict string '" + s + "' using EAPI '" + e.name + "':");

    if (! e.supported)
        throw DepStringParseError(s, "Don't know how to parse EAPI '" + e.name + "' restrictions");

    return parse<RestrictSpecTree, ParseTextDepSpec, false, true, void>(s, false,
            ParseTextDepSpec(), e);
}

tr1::shared_ptr<URISpecTree::ConstItem>
paludis::erepository::parse_uri(const std::string & s, const EAPI & e)
{
    Context c("When parsing URI string '" + s + "' using EAPI '" + e.name + "':");

    if (! e.supported)
        throw DepStringParseError(s, "Don't know how to parse EAPI '" + e.name + "' URIs");

    return parse<URISpecTree, ParseURIDepSpec, false, true, LabelsAreURI>(s, false,
            ParseURIDepSpec(e.supported->uri_supports_arrow), e);
}

tr1::shared_ptr<LicenseSpecTree::ConstItem>
paludis::erepository::parse_license(const std::string & s, const EAPI & e)
{
    Context c("When parsing license string '" + s + "' using EAPI '" + e.name + "':");

    if (! e.supported)
        throw DepStringParseError(s, "Don't know how to parse EAPI '" + e.name + "' licenses");

    return parse<LicenseSpecTree, ParseTextDepSpec, true, true, void>(s,
            true, ParseTextDepSpec(), e);
}

tr1::shared_ptr<LabelsDepSpec<URILabelVisitorTypes> >
paludis::erepository::parse_uri_label(const std::string & s, const EAPI & e)
{
    Context context("When parsing label string '" + s + "' using EAPI '" + e.name + "':");

    if (s.empty())
        throw DepStringParseError(s, "Empty label");

    std::string c(e.supported->uri_labels->class_for_label(s.substr(0, s.length() - 1)));
    if (c.empty())
        throw DepStringParseError(s, "Unknown label");

    tr1::shared_ptr<LabelsDepSpec<URILabelVisitorTypes> > l(new LabelsDepSpec<URILabelVisitorTypes>);

    if (c == "URIMirrorsThenListedLabel")
        l->add_label(make_shared_ptr(new URIMirrorsThenListedLabel(s.substr(0, s.length() - 1))));
    else if (c == "URIMirrorsOnlyLabel")
        l->add_label(make_shared_ptr(new URIMirrorsOnlyLabel(s.substr(0, s.length() - 1))));
    else if (c == "URIListedOnlyLabel")
        l->add_label(make_shared_ptr(new URIListedOnlyLabel(s.substr(0, s.length() - 1))));
    else if (c == "URIListedThenMirrorsLabel")
        l->add_label(make_shared_ptr(new URIListedThenMirrorsLabel(s.substr(0, s.length() - 1))));
    else if (c == "URILocalMirrorsOnlyLabel")
        l->add_label(make_shared_ptr(new URILocalMirrorsOnlyLabel(s.substr(0, s.length() - 1))));
    else if (c == "URIManualOnlyLabel")
        l->add_label(make_shared_ptr(new URIManualOnlyLabel(s.substr(0, s.length() - 1))));
    else
        throw DepStringParseError(s, "Label '" + s + "' maps to unknown class '" + c + "'");

    return l;
}
