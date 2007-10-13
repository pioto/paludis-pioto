/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2007 David Leverton <levertond@googlemail.com>
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

#include "iterator.hh"

#include <paludis/util/join.hh>

#include <test/test_runner.hh>
#include <test/test_framework.hh>

#include <map>
#include <string>
#include <vector>

using namespace test;
using namespace paludis;

namespace test_cases
{
    struct FirstIteratorTest : TestCase
    {
        FirstIteratorTest() : TestCase("first_iterator") {}

        void run()
        {
            typedef std::vector<std::pair<std::string, std::string> > V;

            V v;
            v.push_back(std::pair<std::string, std::string>("one", "I"));
            v.push_back(std::pair<std::string, std::string>("two", "II"));
            v.push_back(std::pair<std::string, std::string>("three", "III"));
            v.push_back(std::pair<std::string, std::string>("four", "IV"));
            v.push_back(std::pair<std::string, std::string>("five", "V"));

            FirstIterator<V::iterator>::Type it = first_iterator(v.begin());
            TEST_CHECK(it == it);
            TEST_CHECK(! (it != it));
            TEST_CHECK_EQUAL(*it, "one");
            TEST_CHECK_EQUAL(it->length(), 3U);

            FirstIterator<V::iterator>::Type it2(it);
            TEST_CHECK(it == it2);
            TEST_CHECK(! (it != it2));
            TEST_CHECK_EQUAL(*++it2, "two");
            TEST_CHECK_EQUAL(*it2, "two");
            TEST_CHECK_EQUAL(it2->length(), 3U);
            TEST_CHECK(it != it2);
            TEST_CHECK(! (it == it2));

            FirstIterator<V::iterator>::Type it3(it2);
            TEST_CHECK(it2 == it3++);
            TEST_CHECK(it2 != it3);
            TEST_CHECK_EQUAL(*it3, "three");
            TEST_CHECK_EQUAL(it3->length(), 5U);

            it3 = it2;
            TEST_CHECK(it2 == it3);
            TEST_CHECK_EQUAL(*it3, "two");
            TEST_CHECK_EQUAL(*it3++, "two");

            TEST_CHECK_EQUAL(join(first_iterator(v.begin()), first_iterator(v.end()), " "), "one two three four five");
        }
    } first_iterator_test;

    struct SecondIteratorTest : TestCase
    {
        SecondIteratorTest() : TestCase("second_iterator") {}

        void run()
        {
            typedef std::map<std::string, std::string> M;

            M m;
            m["I"] = "one";
            m["II"] = "two";
            m["III"] = "three";
            m["IV"] = "four";
            m["V"] = "five";

            SecondIterator<M::iterator>::Type it = second_iterator(m.begin());
            TEST_CHECK(it == it);
            TEST_CHECK(! (it != it));
            TEST_CHECK_EQUAL(*it, "one");
            TEST_CHECK_EQUAL(it->length(), 3U);

            SecondIterator<M::iterator>::Type it2(it);
            TEST_CHECK(it == it2);
            TEST_CHECK(! (it != it2));
            TEST_CHECK_EQUAL(*++it2, "two");
            TEST_CHECK_EQUAL(*it2, "two");
            TEST_CHECK_EQUAL(it2->length(), 3U);
            TEST_CHECK(it != it2);
            TEST_CHECK(! (it == it2));

            SecondIterator<M::iterator>::Type it3(it2);
            TEST_CHECK(it2 == it3++);
            TEST_CHECK(it2 != it3);
            TEST_CHECK_EQUAL(*it3, "three");
            TEST_CHECK_EQUAL(it3->length(), 5U);

            it3 = it2;
            TEST_CHECK(it2 == it3);
            TEST_CHECK_EQUAL(*it3, "two");
            TEST_CHECK_EQUAL(*it3++, "two");

            TEST_CHECK_EQUAL(join(second_iterator(m.begin()), second_iterator(m.end()), " "), "one two three four five");
        }
    } second_iterator_test;
}
