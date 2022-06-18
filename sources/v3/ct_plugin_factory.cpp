#include "ct_plugin_factory.hpp"
#include "ct_component.hpp"
#include "ct_host.hpp"
#include "clap_helpers.hpp"
#include "utility/url_helpers.hpp"
#include "utility/unicode_helpers.hpp"
#include "utility/ct_uuid.hpp"
#include <memory>
#include <cstring>

const ct_plugin_factory::vtable ct_plugin_factory::s_vtable;

ct_plugin_factory::ct_plugin_factory(const clap_plugin_factory *cf)
{
    m_factory = cf;
}

ct_plugin_factory::~ct_plugin_factory()
{
    if (v3::object *host = m_hostcontext)
        host->m_vptr->i_unk.unref(host);
}

v3_result V3_API ct_plugin_factory::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    v3::object *result = nullptr;

    if (!std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) ||
        !std::memcmp(iid, v3_plugin_factory_iid, sizeof(v3_tuid)) ||
        !std::memcmp(iid, v3_plugin_factory_2_iid, sizeof(v3_tuid)) ||
        !std::memcmp(iid, v3_plugin_factory_3_iid, sizeof(v3_tuid)))
    {
        result = (v3::object *)self_;
    }

    if (result) {
        *obj = result;
        result->m_vptr->i_unk.ref(result);
        LOG_PLUGIN_RET(V3_OK);
    }
    else {
        *obj = nullptr;
        LOG_PLUGIN_RET(V3_NO_INTERFACE);
    }
}

uint32_t V3_API ct_plugin_factory::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    LOG_PLUGIN_RET(V3_OK);
}

uint32_t V3_API ct_plugin_factory::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_plugin_factory::get_factory_info(void *self_, v3_factory_info *info)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plugin_factory *self = (ct_plugin_factory *)self_;
    const clap_plugin_factory *cf = self->m_factory;

    const clap_plugin_descriptor *desc0 = CLAP_CALL(cf, get_plugin_descriptor, cf, 0);
    if (!desc0)
        LOG_PLUGIN_RET(V3_FALSE);

    ct::UTF_copy(info->vendor, desc0->vendor);
    ct::UTF_copy(info->url, desc0->url);

    std::string_view support_url{desc0->support_url};
    if (support_url.substr(0, 7) == "mailto:") {
        std::string_view support_email = support_url.substr(7);
        std::size_t pos = support_email.find('?');
        if (pos != support_email.npos)
            support_email = support_email.substr(0, pos);
        ct::UTF_copy(info->email, ct::URL_decode(support_email));
    }
    else {
        info->email[0] = 0;
    }

    info->flags = 0x10; // unicode

    LOG_PLUGIN_RET(V3_OK);
}

int32_t V3_API ct_plugin_factory::num_classes(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plugin_factory *self = (ct_plugin_factory *)self_;
    const clap_plugin_factory *cf = self->m_factory;

    uint32_t count = CLAP_CALL(cf, get_plugin_count, cf);
    LOG_PLUGIN_RET(count);
}

v3_result V3_API ct_plugin_factory::get_class_info(void *self_, int32_t idx, v3_class_info *info)
{
    LOG_PLUGIN_SELF_CALL(self_);

    v3_class_info_2 info2{};
    if (get_class_info_2(self_, idx, &info2) != V3_OK)
        LOG_PLUGIN_RET(V3_FALSE);

    std::memcpy(info->class_id, info2.class_id, sizeof(v3_tuid));
    info->cardinality = info2.cardinality;
    ct::UTF_copy(info->category, info2.category);
    ct::UTF_copy(info->name, info2.name);

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_plugin_factory::create_instance(void *self_, const v3_tuid class_id, const v3_tuid iid, void **instance)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plugin_factory *self = (ct_plugin_factory *)self_;

    if (!std::memcmp(iid, &v3_component_iid, sizeof(v3_tuid))) {
        uint32_t idx = ~0u;

        for (uint32_t i = 0, n = num_classes(self_); i < n && idx == ~0u; ++i) {
            v3_class_info_3 info{};
            if (get_class_info_utf16(self_, i, &info) == V3_OK) {
                if (!std::memcmp(info.class_id, class_id, sizeof(v3_tuid)))
                    idx = i;
            }
        }

        if (idx == ~0u)
            LOG_PLUGIN_RET(V3_FALSE);

        const clap_plugin_factory *cf = self->m_factory;
        const clap_plugin_descriptor *desc = CLAP_CALL(cf, get_plugin_descriptor, cf, idx);

        bool init_ok = false;
        std::unique_ptr<ct_component> comp{new ct_component{class_id, cf, desc, self->m_hostcontext, &init_ok}};
        if (!init_ok)
            LOG_PLUGIN_RET(V3_FALSE);

        *instance = comp.release();
        LOG_PLUGIN_RET(V3_OK);
    }
    else {
        LOG_PLUGIN_RET(V3_FALSE);
    }
}

v3_result V3_API ct_plugin_factory::get_class_info_2(void *self_, int32_t idx, v3_class_info_2 *info)
{
    LOG_PLUGIN_SELF_CALL(self_);

    v3_class_info_3 info3{};
    if (get_class_info_utf16(self_, idx, &info3) != V3_OK)
        LOG_PLUGIN_RET(V3_FALSE);

    std::memcpy(&info->class_id, &info3.class_id, sizeof(v3_tuid));
    info->cardinality = info3.cardinality;
    ct::UTF_copy(info->category, info3.category);
    ct::UTF_copy(info->name, info3.name);
    info->class_flags = info3.class_flags;
    ct::UTF_copy(info->sub_categories, info3.sub_categories);
    ct::UTF_copy(info->vendor, info3.vendor);
    ct::UTF_copy(info->version, info3.version);
    ct::UTF_copy(info->sdk_version, info3.sdk_version);

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_plugin_factory::get_class_info_utf16(void *self_, int32_t idx, v3_class_info_3 *info)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plugin_factory *self = (ct_plugin_factory *)self_;
    const clap_plugin_factory *cf = self->m_factory;

    const clap_plugin_descriptor *desc = CLAP_CALL(cf, get_plugin_descriptor, cf, idx);
    if (!desc)
        LOG_PLUGIN_RET(V3_FALSE);

    ct::generate_uuid(info->class_id, v3_wrapper_namespace_uuid, desc->id);
    info->cardinality = 0x7fffffff; // many instances
    ct::UTF_copy(info->category, "Audio Module Class");
    ct::UTF_copy(info->name, desc->name);
    info->class_flags = 0;
    ct::UTF_copy(info->sub_categories, convert_categories_from_clap(desc->features));
    ct::UTF_copy(info->vendor, desc->vendor);
    ct::UTF_copy(info->version, desc->version);
    ct::UTF_copy(info->sdk_version, "Travesty 3.7.4");

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_plugin_factory::set_host_context(void *self_, v3_funknown **host_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plugin_factory *self = (ct_plugin_factory *)self_;
    v3::object *host = (v3::object *)host_;

    if (self->m_hostcontext != host) {
        v3::object *old = self->m_hostcontext;
        if (old)
            old->m_vptr->i_unk.unref(old);
        if (host)
            host->m_vptr->i_unk.ref(host);
        self->m_hostcontext = host;
    }

    LOG_PLUGIN_RET(V3_OK);
}
