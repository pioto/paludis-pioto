/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2005, 2006, 2007 Ciaran McCreesh
 * Copyright (c) 2007 David Leverton
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

#include <paludis/util/indirect_iterator-impl.hh>
#include <test/test_runner.hh>
#include <test/test_framework.hh>
#include <vector>
#include <list>

using namespace test;
using namespace paludis;

namespace
{
    struct Deleter
    {
        template <typename T_>
        void operator() (T_ t)
        {
            delete t;
        }
    };
}

namespace test_cases
{
    struct IndirectIteratorVecCPIntTest : TestCase
    {
        IndirectIteratorVecCPIntTest() : TestCase("vector<tr1::shared_ptr<int> >") { }

        void run()
        {
            std::vector<tr1::shared_ptr<int> > v;
            v.push_back(tr1::shared_ptr<int>(new int(5)));
            v.push_back(tr1::shared_ptr<int>(new int(10)));
            IndirectIterator<std::vector<tr1::shared_ptr<int> >::iterator, int> vi(v.begin()), vi_end(v.end());
            TEST_CHECK(vi != vi_end);
            TEST_CHECK(vi < vi_end);
            TEST_CHECK(! (vi > vi_end));
            TEST_CHECK_EQUAL(*vi, 5);
            TEST_CHECK(++vi != vi_end);
            TEST_CHECK(vi < vi_end);
            TEST_CHECK(! (vi > vi_end));
            TEST_CHECK_EQUAL(*vi, 10);
            TEST_CHECK(++vi == vi_end);
        }
    } test_indirect_iterator_vec_cp_int;

    /**
     * \test Test IndirectIterator over a list of shared_ptr of int.
     *
     */
    struct IndirectIteratorListCPIntTest : TestCase
    {
        IndirectIteratorListCPIntTest() : TestCase("list<tr1::shared_ptr<int> >") { }

        void run()
        {
            std::list<tr1::shared_ptr<int> > v;
            v.push_back(tr1::shared_ptr<int>(new int(5)));
            v.push_back(tr1::shared_ptr<int>(new int(10)));
            IndirectIterator<std::list<tr1::shared_ptr<int> >::iterator> vi(v.begin()), vi_end(v.end());
            TEST_CHECK(vi != vi_end);
            TEST_CHECK_EQUAL(*vi, 5);
            TEST_CHECK(++vi != vi_end);
            TEST_CHECK_EQUAL(*vi, 10);
            TEST_CHECK(++vi == vi_end);
        }
    } test_indirect_iterator_list_cp_int;

    /**
     * \test Test IndirectIterator over a vector of int *.
     *
     */
    struct IndirectIteratorVecPIntTest : TestCase
    {
        IndirectIteratorVecPIntTest() : TestCase("vector<int *>") { }

        void run()
        {
            std::vector<int *> v;
            v.push_back(new int(5));
            v.push_back(new int(10));
            IndirectIterator<std::vector<int *>::iterator, int> vi(v.begin()), vi_end(v.end());
            TEST_CHECK(vi != vi_end);
            TEST_CHECK(vi < vi_end);
            TEST_CHECK(! (vi > vi_end));
            TEST_CHECK_EQUAL(*vi, 5);
            TEST_CHECK(++vi != vi_end);
            TEST_CHECK(vi < vi_end);
            TEST_CHECK(! (vi > vi_end));
            TEST_CHECK_EQUAL(*vi, 10);
            TEST_CHECK(++vi == vi_end);

            std::for_each(v.begin(), v.end(), Deleter());
        }
    } test_indirect_iterator_vec_p_int;

    /**
     * \test Test IndirectIterator over a list of int *.
     *
     */
    struct IndirectIteratorListPIntTest : TestCase
    {
        IndirectIteratorListPIntTest() : TestCase("list<int *>") { }

        void run()
        {
            std::list<int *> v;
            v.push_back(new int(5));
            v.push_back(new int(10));
            IndirectIterator<std::list<int *>::iterator, int> vi(v.begin()), vi_end(v.end());
            TEST_CHECK(vi != vi_end);
            TEST_CHECK_EQUAL(*vi, 5);
            TEST_CHECK(++vi != vi_end);
            TEST_CHECK_EQUAL(*vi, 10);
            TEST_CHECK(++vi == vi_end);

            std::for_each(v.begin(), v.end(), Deleter());
        }
    } test_indirect_iterator_list_p_int;
}
