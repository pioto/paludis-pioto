/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2009 Ciaran McCreesh
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

#include <paludis/util/simple_parser.hh>
#include <test/test_runner.hh>
#include <test/test_framework.hh>
#include <test/test_concepts.hh>

using namespace paludis;
using namespace test;

namespace test_cases
{
    struct TestSimpleParserIgnoreCase : TestCase
    {
        TestSimpleParserIgnoreCase() : TestCase("ignore case") { }

        void run()
        {
            std::string text("oneTWOthree"), one, two, three;
            SimpleParser parser(text);
            TEST_CHECK(parser.consume(simple_parser::exact("one") >> one));
            TEST_CHECK(parser.consume(simple_parser::exact_ignoring_case("two") >> two));
            TEST_CHECK(! parser.consume(simple_parser::exact("THREE") >> three));
            TEST_CHECK(parser.consume(simple_parser::exact_ignoring_case("THREE") >> three));

            TEST_CHECK(parser.eof());
            TEST_CHECK_EQUAL(one, "one");
            TEST_CHECK_EQUAL(two, "TWO");
            TEST_CHECK_EQUAL(three, "three");
        }
    } test_ignore_case;
}

