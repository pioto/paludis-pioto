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

#ifndef PALUDIS_GUARD_PALUDIS_UTIL_TAIL_OUTPUT_STREAM_HH
#define PALUDIS_GUARD_PALUDIS_UTIL_TAIL_OUTPUT_STREAM_HH 1

#include <paludis/util/tail_output_stream-fwd.hh>
#include <paludis/util/attributes.hh>
#include <paludis/util/private_implementation_pattern.hh>
#include <paludis/util/sequence.hh>
#include <tr1/memory>
#include <ostream>

namespace paludis
{
    class PALUDIS_VISIBLE TailOutputStreamBuf :
        public std::streambuf,
        private PrivateImplementationPattern<TailOutputStreamBuf>
    {
        private:
            void _append(const std::string &);

        protected:
            virtual int_type
            overflow(int_type c);

            virtual std::streamsize
            xsputn(const char * s, std::streamsize num);

        public:
            TailOutputStreamBuf(const unsigned n);
            ~TailOutputStreamBuf();

            const std::tr1::shared_ptr<const Sequence<std::string> > tail(const bool clear);
    };

    class PALUDIS_VISIBLE TailOutputStreamBase
    {
        protected:
            TailOutputStreamBuf buf;

        public:
            TailOutputStreamBase(const unsigned n) :
                buf(n)
            {
            }
    };

    class PALUDIS_VISIBLE TailOutputStream :
        protected TailOutputStreamBase,
        public std::ostream
    {
        public:
            ///\name Basic operations
            ///\{

            TailOutputStream(const unsigned n) :
                TailOutputStreamBase(n),
                std::ostream(&buf)
            {
            }

            const std::tr1::shared_ptr<const Sequence<std::string> > tail(const bool clear_after)
            {
                return buf.tail(clear_after);
            }

            ///\}
    };

#ifdef PALUDIS_HAVE_EXTERN_TEMPLATE
    extern template class PrivateImplementationPattern<TailOutputStreamBuf>;
#endif
}

#endif