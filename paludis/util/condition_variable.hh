/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2007 Ciaran McCreesh <ciaranm@ciaranm.org>
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

#ifndef PALUDIS_GUARD_PALUDIS_UTIL_CONDITION_VARIABLE_HH
#define PALUDIS_GUARD_PALUDIS_UTIL_CONDITION_VARIABLE_HH 1

#include <paludis/util/attributes.hh>
#include <paludis/util/mutex.hh>

#ifdef PALUDIS_ENABLE_THREADS
#  include <pthread.h>
#endif

namespace paludis
{
    class PALUDIS_VISIBLE ConditionVariable
    {
        private:
            ConditionVariable(const ConditionVariable &);
            ConditionVariable & operator= (const ConditionVariable &);

#ifdef PALUDIS_ENABLE_THREADS
            pthread_cond_t * const _cond;
#endif

        public:
            ConditionVariable();
            ~ConditionVariable();

            void broadcast();
            void signal();
            void acquire_then_signal(Mutex &);

            void wait(Mutex &);
    };
}

#endif