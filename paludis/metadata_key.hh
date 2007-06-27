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

#ifndef PALUDIS_GUARD_PALUDIS_METADATA_KEY_HH
#define PALUDIS_GUARD_PALUDIS_METADATA_KEY_HH 1

#include <paludis/metadata_key-fwd.hh>
#include <paludis/package_id-fwd.hh>
#include <paludis/name-fwd.hh>
#include <paludis/dep_spec-fwd.hh>
#include <paludis/util/attributes.hh>
#include <paludis/util/instantiation_policy.hh>
#include <paludis/util/private_implementation_pattern.hh>
#include <paludis/util/visitor.hh>
#include <string>

namespace paludis
{
    struct MetadataKeyVisitorTypes :
        VisitorTypes<
            MetadataKeyVisitorTypes,
            MetadataKey,
            MetadataPackageIDKey,
            MetadataCollectionKey<UseFlagNameCollection>,
            MetadataCollectionKey<IUseFlagCollection>,
            MetadataCollectionKey<KeywordNameCollection>,
            MetadataCollectionKey<InheritedCollection>,
            MetadataSpecTreeKey<DependencySpecTree>,
            MetadataSpecTreeKey<LicenseSpecTree>,
            MetadataSpecTreeKey<URISpecTree>,
            MetadataSpecTreeKey<ProvideSpecTree>,
            MetadataSpecTreeKey<RestrictSpecTree>,
            MetadataStringKey
            >
    {
    };

    class PALUDIS_VISIBLE MetadataKey :
        private InstantiationPolicy<MetadataKey, instantiation_method::NonCopyableTag>,
        private PrivateImplementationPattern<MetadataKey>,
        public virtual ConstAcceptInterface<MetadataKeyVisitorTypes>
    {
        protected:
            MetadataKey(const std::string &, const std::string &);

        public:
            virtual ~MetadataKey() = 0;

            virtual const std::string raw_name() const PALUDIS_ATTRIBUTE((warn_unused_result));
            virtual const std::string human_name() const PALUDIS_ATTRIBUTE((warn_unused_result));
    };

    class PALUDIS_VISIBLE MetadataPackageIDKey :
        public MetadataKey,
        public ConstAcceptInterfaceVisitsThis<MetadataKeyVisitorTypes, MetadataPackageIDKey>
    {
        protected:
            MetadataPackageIDKey(const std::string &, const std::string &);

        public:
            virtual const tr1::shared_ptr<const PackageID> value() const
                PALUDIS_ATTRIBUTE((warn_unused_result)) = 0;
    };

    class PALUDIS_VISIBLE MetadataStringKey :
        public MetadataKey,
        public ConstAcceptInterfaceVisitsThis<MetadataKeyVisitorTypes, MetadataStringKey>
    {
        protected:
            MetadataStringKey(const std::string &, const std::string &);

        public:
            virtual const std::string value() const
                PALUDIS_ATTRIBUTE((warn_unused_result)) = 0;
    };

    template <typename C_>
    class PALUDIS_VISIBLE MetadataCollectionKey :
        public MetadataKey,
        public ConstAcceptInterfaceVisitsThis<MetadataKeyVisitorTypes, MetadataCollectionKey<C_> >
    {
        protected:
            MetadataCollectionKey(const std::string &, const std::string &);

        public:
            virtual const tr1::shared_ptr<const C_> value() const
                PALUDIS_ATTRIBUTE((warn_unused_result)) = 0;
    };

    template <typename C_>
    class PALUDIS_VISIBLE MetadataSpecTreeKey :
        public MetadataKey,
        public ConstAcceptInterfaceVisitsThis<MetadataKeyVisitorTypes, MetadataSpecTreeKey<C_> >
    {
        protected:
            MetadataSpecTreeKey(const std::string &, const std::string &);

        public:
            virtual const tr1::shared_ptr<const typename C_::ConstItem> value() const
                PALUDIS_ATTRIBUTE((warn_unused_result)) = 0;
    };
}

#endif