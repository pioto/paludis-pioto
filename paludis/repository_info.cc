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

#include <paludis/name.hh>
#include <paludis/repository.hh>
#include <paludis/repository_info.hh>
#include <paludis/util/private_implementation_pattern-impl.hh>
#include <paludis/util/instantiation_policy-impl.hh>
#include <paludis/util/stringify.hh>
#include <paludis/util/collection_concrete.hh>
#include <libwrapiter/libwrapiter_forward_iterator.hh>
#include <libwrapiter/libwrapiter_output_iterator.hh>
#include <list>
#include <map>
#include <utility>

using namespace paludis;

namespace paludis
{
    /**
     * Implementation data for RepositoryInfoSection.
     *
     * \ingroup grprepository
     */
    template<>
    struct Implementation<RepositoryInfoSection>
    {
        std::string heading;
        std::map<std::string, std::string> kvs;
    };

    /**
     * Implementation data for RepositoryInfo.
     *
     * \ingroup grprepository
     */
    template<>
    struct Implementation<RepositoryInfo>
    {
        std::list<tr1::shared_ptr<const RepositoryInfoSection> > sections;
    };
}

RepositoryInfo &
RepositoryInfo::add_section(tr1::shared_ptr<const RepositoryInfoSection> s)
{
    _imp->sections.push_back(s);
    return *this;
}

RepositoryInfoSection::RepositoryInfoSection(const std::string & our_heading) :
    PrivateImplementationPattern<RepositoryInfoSection>(new Implementation<RepositoryInfoSection>)
{
    _imp->heading = our_heading;
}

RepositoryInfoSection::~RepositoryInfoSection()
{
}

std::string
RepositoryInfoSection::heading() const
{
    return _imp->heading;
}

RepositoryInfoSection::KeyValueIterator
RepositoryInfoSection::begin_kvs() const
{
    return KeyValueIterator(_imp->kvs.begin());
}

RepositoryInfoSection::KeyValueIterator
RepositoryInfoSection::end_kvs() const
{
    return KeyValueIterator(_imp->kvs.end());
}

RepositoryInfoSection &
RepositoryInfoSection::add_kv(const std::string & k, const std::string & v)
{
    _imp->kvs.insert(std::make_pair(k, v));
    return *this;
}

std::string
RepositoryInfoSection::get_key_with_default(const std::string & k, const std::string & d) const
{
    std::map<std::string, std::string>::const_iterator p(_imp->kvs.find(k));
    if (p != _imp->kvs.end())
        return p->second;
    else
        return d;
}

RepositoryInfo::RepositoryInfo() :
    PrivateImplementationPattern<RepositoryInfo>(new Implementation<RepositoryInfo>)
{
}

RepositoryInfo::~RepositoryInfo()
{
}

RepositoryInfo::SectionIterator
RepositoryInfo::begin_sections() const
{
    return SectionIterator(_imp->sections.begin());
}

RepositoryInfo::SectionIterator
RepositoryInfo::end_sections() const
{
    return SectionIterator(_imp->sections.end());
}

std::string
RepositoryInfo::get_key_with_default(const std::string & k, const std::string & d) const
{
    std::string result(d);
    for (SectionIterator i(begin_sections()), i_end(end_sections()) ; i != i_end ; ++i)
        result = (*i)->get_key_with_default(k, result);
    return result;
}
