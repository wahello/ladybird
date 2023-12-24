/*
 * Copyright (c) 2021, Tim Flynn <trflynn89@serenityos.org>
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2022, Alexander Narsudinov <a.narsudinov@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/DOM/Attr.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/NamedNodeMap.h>
#include <LibWeb/Infra/Strings.h>
#include <LibWeb/Namespace.h>

namespace Web::DOM {

JS_DEFINE_ALLOCATOR(NamedNodeMap);

JS::NonnullGCPtr<NamedNodeMap> NamedNodeMap::create(Element& element)
{
    auto& realm = element.realm();
    return realm.heap().allocate<NamedNodeMap>(realm, element);
}

NamedNodeMap::NamedNodeMap(Element& element)
    : Bindings::LegacyPlatformObject(element.realm())
    , m_element(element)
{
}

void NamedNodeMap::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    set_prototype(&Bindings::ensure_web_prototype<Bindings::NamedNodeMapPrototype>(realm, "NamedNodeMap"_fly_string));
}

void NamedNodeMap::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_element);
    for (auto& attribute : m_attributes)
        visitor.visit(attribute);
}

// https://dom.spec.whatwg.org/#ref-for-dfn-supported-property-indices%E2%91%A3
bool NamedNodeMap::is_supported_property_index(u32 index) const
{
    return index < m_attributes.size();
}

// https://dom.spec.whatwg.org/#ref-for-dfn-supported-property-names%E2%91%A0
Vector<String> NamedNodeMap::supported_property_names() const
{
    // 1. Let names be the qualified names of the attributes in this NamedNodeMap object’s attribute list, with duplicates omitted, in order.
    Vector<String> names;
    names.ensure_capacity(m_attributes.size());

    for (auto const& attribute : m_attributes) {
        auto const attribute_name = attribute->name();
        if (!names.contains_slow(attribute_name))
            names.append(attribute_name.to_string());
    }

    // 2. If this NamedNodeMap object’s element is in the HTML namespace and its node document is an HTML document, then for each name in names:
    // FIXME: Handle the second condition, assume it is an HTML document for now.
    if (associated_element().namespace_uri() == Namespace::HTML) {
        // 1. Let lowercaseName be name, in ASCII lowercase.
        // 2. If lowercaseName is not equal to name, remove name from names.
        names.remove_all_matching([](auto const& name) { return name != MUST(Infra::to_ascii_lowercase(name)); });
    }

    // 3. Return names.
    return names;
}

// https://dom.spec.whatwg.org/#dom-namednodemap-item
Attr const* NamedNodeMap::item(u32 index) const
{
    // 1. If index is equal to or greater than this’s attribute list’s size, then return null.
    if (index >= m_attributes.size())
        return nullptr;

    // 2. Otherwise, return this’s attribute list[index].
    return m_attributes[index].ptr();
}

// https://dom.spec.whatwg.org/#dom-namednodemap-getnameditem
Attr const* NamedNodeMap::get_named_item(StringView qualified_name) const
{
    return get_attribute(qualified_name);
}

// https://dom.spec.whatwg.org/#dom-namednodemap-getnameditemns
Attr const* NamedNodeMap::get_named_item_ns(Optional<String> const& namespace_, StringView local_name) const
{
    // FIXME: This conversion is quite ugly.
    StringView namespace_view;
    if (namespace_.has_value())
        namespace_view = namespace_->bytes_as_string_view();

    return get_attribute_ns(namespace_view, local_name);
}

// https://dom.spec.whatwg.org/#dom-namednodemap-setnameditem
WebIDL::ExceptionOr<JS::GCPtr<Attr>> NamedNodeMap::set_named_item(Attr& attribute)
{
    return set_attribute(attribute);
}

// https://dom.spec.whatwg.org/#dom-namednodemap-setnameditemns
WebIDL::ExceptionOr<JS::GCPtr<Attr>> NamedNodeMap::set_named_item_ns(Attr& attribute)
{
    return set_attribute(attribute);
}

// https://dom.spec.whatwg.org/#dom-namednodemap-removenameditem
WebIDL::ExceptionOr<Attr const*> NamedNodeMap::remove_named_item(StringView qualified_name)
{
    // 1. Let attr be the result of removing an attribute given qualifiedName and element.
    auto const* attribute = remove_attribute(qualified_name);

    // 2. If attr is null, then throw a "NotFoundError" DOMException.
    if (!attribute)
        return WebIDL::NotFoundError::create(realm(), MUST(String::formatted("Attribute with name '{}' not found", qualified_name)));

    // 3. Return attr.
    return attribute;
}

// https://dom.spec.whatwg.org/#dom-namednodemap-removenameditemns
WebIDL::ExceptionOr<Attr const*> NamedNodeMap::remove_named_item_ns(Optional<String> const& namespace_, StringView local_name)
{
    // FIXME: This conversion is quite ugly.
    StringView namespace_view;
    if (namespace_.has_value())
        namespace_view = namespace_->bytes_as_string_view();

    // 1. Let attr be the result of removing an attribute given namespace, localName, and element.
    auto const* attribute = remove_attribute_ns(namespace_view, local_name);

    // 2. If attr is null, then throw a "NotFoundError" DOMException.
    if (!attribute)
        return WebIDL::NotFoundError::create(realm(), MUST(String::formatted("Attribute with namespace '{}' and local name '{}' not found", namespace_, local_name)));

    // 3. Return attr.
    return attribute;
}

// https://dom.spec.whatwg.org/#concept-element-attributes-get-by-name
Attr* NamedNodeMap::get_attribute(StringView qualified_name, size_t* item_index)
{
    return const_cast<Attr*>(const_cast<NamedNodeMap const*>(this)->get_attribute(qualified_name, item_index));
}

// https://dom.spec.whatwg.org/#concept-element-attributes-get-by-name
Attr const* NamedNodeMap::get_attribute(StringView qualified_name, size_t* item_index) const
{
    if (item_index)
        *item_index = 0;

    // 1. If element is in the HTML namespace and its node document is an HTML document, then set qualifiedName to qualifiedName in ASCII lowercase.
    // FIXME: Handle the second condition, assume it is an HTML document for now.
    bool compare_as_lowercase = associated_element().namespace_uri() == Namespace::HTML;

    // 2. Return the first attribute in element’s attribute list whose qualified name is qualifiedName; otherwise null.
    for (auto const& attribute : m_attributes) {
        if (compare_as_lowercase) {
            if (attribute->name().equals_ignoring_ascii_case(qualified_name))
                return attribute.ptr();
        } else {
            if (attribute->name() == qualified_name)
                return attribute.ptr();
        }

        if (item_index)
            ++(*item_index);
    }

    return nullptr;
}

// https://dom.spec.whatwg.org/#concept-element-attributes-get-by-namespace
Attr const* NamedNodeMap::get_attribute_ns(Optional<FlyString> const& namespace_, FlyString const& local_name, size_t* item_index) const
{
    // FIXME: We shouldn't need to do any conversion when looking up in the node map.
    StringView namespace_view;
    if (namespace_.has_value())
        namespace_view = namespace_->bytes_as_string_view();

    return get_attribute_ns(namespace_view, local_name, item_index);
}

// https://dom.spec.whatwg.org/#concept-element-attributes-get-by-namespace
Attr* NamedNodeMap::get_attribute_ns(Optional<FlyString> const& namespace_, FlyString const& local_name, size_t* item_index)
{
    return const_cast<Attr*>(const_cast<NamedNodeMap const*>(this)->get_attribute_ns(namespace_, local_name, item_index));
}

// https://dom.spec.whatwg.org/#concept-element-attributes-get-by-namespace
Attr* NamedNodeMap::get_attribute_ns(StringView namespace_, StringView local_name, size_t* item_index)
{
    return const_cast<Attr*>(const_cast<NamedNodeMap const*>(this)->get_attribute_ns(namespace_, local_name, item_index));
}

// https://dom.spec.whatwg.org/#concept-element-attributes-get-by-namespace
Attr const* NamedNodeMap::get_attribute_ns(StringView namespace_, StringView local_name, size_t* item_index) const
{
    if (item_index)
        *item_index = 0;

    // 1. If namespace is the empty string, then set it to null.
    if (namespace_.is_empty())
        namespace_ = {};

    // 2. Return the attribute in element’s attribute list whose namespace is namespace and local name is localName, if any; otherwise null.
    for (auto const& attribute : m_attributes) {
        // FIXME: This is quite awkard. We should probably be taking an Optional<FlyString> for namespace here.
        if ((!attribute->namespace_uri().has_value() == namespace_.is_null() || attribute->namespace_uri() == namespace_) && attribute->local_name() == local_name)
            return attribute.ptr();
        if (item_index)
            ++(*item_index);
    }

    return nullptr;
}

// https://dom.spec.whatwg.org/#concept-element-attributes-set
WebIDL::ExceptionOr<JS::GCPtr<Attr>> NamedNodeMap::set_attribute(Attr& attribute)
{
    // 1. If attr’s element is neither null nor element, throw an "InUseAttributeError" DOMException.
    if ((attribute.owner_element() != nullptr) && (attribute.owner_element() != &associated_element()))
        return WebIDL::InUseAttributeError::create(realm(), "Attribute must not already be in use"_fly_string);

    // 2. Let oldAttr be the result of getting an attribute given attr’s namespace, attr’s local name, and element.
    size_t old_attribute_index = 0;
    auto* old_attribute = get_attribute_ns(attribute.namespace_uri(), attribute.local_name(), &old_attribute_index);

    // 3. If oldAttr is attr, return attr.
    if (old_attribute == &attribute)
        return &attribute;

    // 4. If oldAttr is non-null, then replace oldAttr with attr.
    if (old_attribute) {
        replace_attribute(*old_attribute, attribute, old_attribute_index);
    }
    // 5. Otherwise, append attr to element.
    else {
        append_attribute(attribute);
    }

    // 6. Return oldAttr.
    return old_attribute;
}

// https://dom.spec.whatwg.org/#concept-element-attributes-replace
void NamedNodeMap::replace_attribute(Attr& old_attribute, Attr& new_attribute, size_t old_attribute_index)
{
    VERIFY(old_attribute.owner_element());

    // 1. Replace oldAttr by newAttr in oldAttr’s element’s attribute list.
    m_attributes.remove(old_attribute_index);
    m_attributes.insert(old_attribute_index, new_attribute);

    // 2. Set newAttr’s element to oldAttr’s element.
    new_attribute.set_owner_element(old_attribute.owner_element());

    // 3. Set oldAttr’s element to null.
    old_attribute.set_owner_element(nullptr);

    // 4. Handle attribute changes for oldAttr with newAttr’s element, oldAttr’s value, and newAttr’s value.
    old_attribute.handle_attribute_changes(*new_attribute.owner_element(), old_attribute.value(), new_attribute.value());
}

// https://dom.spec.whatwg.org/#concept-element-attributes-append
void NamedNodeMap::append_attribute(Attr& attribute)
{
    // 1. Append attribute to element’s attribute list.
    m_attributes.append(attribute);

    // 2. Set attribute’s element to element.
    attribute.set_owner_element(&associated_element());

    // 3. Handle attribute changes for attribute with element, null, and attribute’s value.
    attribute.handle_attribute_changes(associated_element(), {}, attribute.value());
}

// https://dom.spec.whatwg.org/#concept-element-attributes-remove
void NamedNodeMap::remove_attribute_at_index(size_t attribute_index)
{
    JS::NonnullGCPtr<Attr> attribute = m_attributes.at(attribute_index);

    // 1. Let element be attribute’s element.
    auto* element = attribute->owner_element();
    VERIFY(element);

    // 2. Remove attribute from element’s attribute list.
    m_attributes.remove(attribute_index);

    // 3. Set attribute’s element to null.
    attribute->set_owner_element(nullptr);

    // 4. Handle attribute changes for attribute with element, attribute’s value, and null.
    attribute->handle_attribute_changes(*element, attribute->value(), {});
}

// https://dom.spec.whatwg.org/#concept-element-attributes-remove-by-name
Attr const* NamedNodeMap::remove_attribute(StringView qualified_name)
{
    size_t item_index = 0;

    // 1. Let attr be the result of getting an attribute given qualifiedName and element.
    auto const* attribute = get_attribute(qualified_name, &item_index);

    // 2. If attr is non-null, then remove attr.
    if (attribute)
        remove_attribute_at_index(item_index);

    // 3. Return attr.
    return attribute;
}

// https://dom.spec.whatwg.org/#concept-element-attributes-remove-by-namespace
Attr const* NamedNodeMap::remove_attribute_ns(StringView namespace_, StringView local_name)
{
    size_t item_index = 0;

    // 1. Let attr be the result of getting an attribute given namespace, localName, and element.
    auto const* attribute = get_attribute_ns(namespace_, local_name, &item_index);

    // 2. If attr is non-null, then remove attr.
    if (attribute)
        remove_attribute_at_index(item_index);

    // 3. Return attr.
    return attribute;
}

WebIDL::ExceptionOr<JS::Value> NamedNodeMap::item_value(size_t index) const
{
    auto const* node = item(index);
    if (!node)
        return JS::js_undefined();
    return node;
}

WebIDL::ExceptionOr<JS::Value> NamedNodeMap::named_item_value(FlyString const& name) const
{
    auto const* node = get_named_item(name);
    if (!node)
        return JS::js_undefined();
    return node;
}

}
