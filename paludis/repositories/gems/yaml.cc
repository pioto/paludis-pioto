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

#include "yaml.hh"
#include <paludis/util/private_implementation_pattern-impl.hh>
#include <paludis/util/instantiation_policy-impl.hh>
#include <paludis/util/visitor-impl.hh>
#include <paludis/util/tr1_functional.hh>
#include <paludis/util/make_shared_ptr.hh>
#include <libwrapiter/libwrapiter_forward_iterator.hh>
#include <libwrapiter/libwrapiter_output_iterator.hh>
#include <syck.h>
#include <cstring>
#include <algorithm>

using namespace paludis;
using namespace paludis::yaml;

Node::~Node()
{
}

namespace paludis
{
    template <>
    struct Implementation<StringNode>
    {
        const std::string text;

        Implementation(const std::string & t) :
            text(t)
        {
        }
    };
}

StringNode::StringNode(const std::string & t) :
    PrivateImplementationPattern<StringNode>(new Implementation<StringNode>(t))
{
}

StringNode::~StringNode()
{
}

std::string
StringNode::text() const
{
    return _imp->text;
}

namespace paludis
{
    template <>
    struct Implementation<SequenceNode>
    {
        std::list<const Node *> nodes;
    };
}

SequenceNode::SequenceNode() :
    PrivateImplementationPattern<SequenceNode>(new Implementation<SequenceNode>)
{
}

SequenceNode::~SequenceNode()
{
}

void
SequenceNode::push_back(const Node * const n)
{
    _imp->nodes.push_back(n);
}

SequenceNode::Iterator
SequenceNode::begin() const
{
    return Iterator(_imp->nodes.begin());
}

SequenceNode::Iterator
SequenceNode::end() const
{
    return Iterator(_imp->nodes.end());
}

namespace paludis
{
    template <>
    struct Implementation<MapNode>
    {
        std::list<std::pair<const Node *, const Node *> > nodes;
    };
}

MapNode::MapNode() :
    PrivateImplementationPattern<MapNode>(new Implementation<MapNode>)
{
}

MapNode::~MapNode()
{
}

void
MapNode::push_back(const std::pair<const Node *, const Node *> & p)
{
    _imp->nodes.push_back(p);
}

MapNode::Iterator
MapNode::begin() const
{
    return Iterator(_imp->nodes.begin());
}

MapNode::Iterator
MapNode::end() const
{
    return Iterator(_imp->nodes.end());
}

namespace
{
    struct MatchStringVisitor :
        ConstVisitor<NodeVisitorTypes>
    {
        bool found;
        const std::string target;

        MatchStringVisitor(const std::string & s) :
            found(false),
            target(s)
        {
        }

        void visit(const StringNode & n)
        {
            found = n.text() == target;
        }

        void visit(const MapNode &)
        {
        }

        void visit(const SequenceNode &)
        {
        }
    };

    bool match_string_node(const std::string & s, const Node * const n)
    {
        MatchStringVisitor v(s);
        n->accept(v);
        return v.found;
    }
}

MapNode::Iterator
MapNode::find(const std::string & s) const
{
    using namespace tr1::placeholders;
    return std::find_if(begin(), end(),
            tr1::bind(match_string_node, s, tr1::bind<const Node *>(tr1::mem_fn(&std::pair<const Node *, const Node *>::first), _1)));
}

namespace
{
    static std::map<void *, std::string> document_error_table;

    template <typename R_, typename T_>
    struct CallUnlessNull
    {
        R_ (* function) (T_ *);

        CallUnlessNull(R_ (*f) (T_ *)) :
            function(f)
        {
        }

        void operator() (T_ * const t) const
        {
            if (t)
                function(t);
        }
    };

    template <typename R_, typename T_>
    CallUnlessNull<R_, T_>
    call_unless_null(R_ (* f) (T_ *))
    {
        return CallUnlessNull<R_, T_>(f);
    }

    SYMID node_handler(SyckParser * p, SyckNode * n)
    {
        Node * node(0);

        switch (n->kind)
        {
            case syck_str_kind:
                {
                    node = new StringNode(std::string(n->data.str->ptr, n->data.str->len));
                    NodeManager::get_instance()->manage_node(p, node);
                }
                break;

            case syck_seq_kind:
                {
                    SequenceNode * s(new SequenceNode);
                    NodeManager::get_instance()->manage_node(p, s);
                    for (int i = 0 ; i < n->data.list->idx ; ++i)
                    {
                        SYMID v_id(syck_seq_read(n, i));
                        char * v(0);
                        syck_lookup_sym(p, v_id, &v);
                        s->push_back(reinterpret_cast<Node *>(v));
                    }
                    node = s;
                }
                break;

            case syck_map_kind:
                {
                    MapNode * m(new MapNode);
                    NodeManager::get_instance()->manage_node(p, m);
                    for (int i = 0 ; i < n->data.pairs->idx ; ++i)
                    {
                        SYMID k_id(syck_map_read(n, map_key, i)), v_id(syck_map_read(n, map_value, i));
                        char * k(0), * v(0);
                        syck_lookup_sym(p, k_id, &k);
                        syck_lookup_sym(p, v_id, &v);
                        m->push_back(std::make_pair(reinterpret_cast<Node *>(k), reinterpret_cast<Node *>(v)));
                    }
                    node = m;
                }
                break;
        }

        return syck_add_sym(p, reinterpret_cast<char *>(node));
    }

    void error_handler(SyckParser * p, char * s)
    {
        document_error_table[p] = s;
    }
}

namespace paludis
{
    template <>
    struct Implementation<Document>
    {
        struct Register
        {
            Implementation<Document> * _imp;

            Register(Implementation<Document> * imp) :
                _imp(imp)
            {
                NodeManager::get_instance()->register_document(_imp->parser.get());
            }

            ~Register()
            {
                NodeManager::get_instance()->deregister_document(_imp->parser.get());
            }
        };

        Node * top;
        tr1::shared_ptr<SyckParser> parser;
        tr1::shared_ptr<char> data;
        unsigned data_length;

        Register reg;

        Implementation(const std::string & s) :
            top(0),
            parser(syck_new_parser(), call_unless_null(syck_free_parser)),
            data(strdup(s.c_str()), call_unless_null(std::free)),
            data_length(s.length()),
            reg(this)
        {
        }
    };
}

Document::Document(const std::string & s) :
    PrivateImplementationPattern<Document>(new Implementation<Document>(s))
{
    Context c("When parsing yaml document:");

    syck_parser_str(_imp->parser.get(), _imp->data.get(), _imp->data_length, 0);
    syck_parser_handler(_imp->parser.get(), node_handler);
    syck_parser_error_handler(_imp->parser.get(), error_handler);

    SYMID root_id(syck_parse(_imp->parser.get()));

    if (document_error_table.end() != document_error_table.find(_imp->parser.get()))
    {
        std::string e(document_error_table.find(_imp->parser.get())->second);
        document_error_table.erase(_imp->parser.get());
        throw ParseError(e);
    }

    char * root_uncasted(0);
    syck_lookup_sym(_imp->parser.get(), root_id, &root_uncasted);
    _imp->top = reinterpret_cast<Node *>(root_uncasted);
}

Document::~Document()
{
}

const Node *
Document::top() const
{
    return _imp->top;
}

namespace paludis
{
    template <>
    struct Implementation<NodeManager>
    {
        std::map<const void *, std::list<tr1::shared_ptr<const Node> > > store;
    };
}

NodeManager::NodeManager() :
    PrivateImplementationPattern<NodeManager>(new Implementation<NodeManager>)
{
}

NodeManager::~NodeManager()
{
}

void
NodeManager::register_document(const void * const d)
{
    if (! _imp->store.insert(std::make_pair(d, std::list<tr1::shared_ptr<const Node> >())).second)
        throw InternalError(PALUDIS_HERE, "duplicate document");
}

void
NodeManager::deregister_document(const void * const d)
{
    if (0 == _imp->store.erase(d))
        throw InternalError(PALUDIS_HERE, "no such document");
}

void
NodeManager::manage_node(const void * const d, const Node * const n)
{
    std::map<const void *, std::list<tr1::shared_ptr<const Node> > >::iterator i(_imp->store.find(d));
    if (i == _imp->store.end())
        throw InternalError(PALUDIS_HERE, "no such document");
    i->second.push_back(make_shared_ptr(n));
}

ParseError::ParseError(const std::string & s) throw () :
    Exception(s)
{
}
