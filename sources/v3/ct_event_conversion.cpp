#include "ct_event_conversion.hpp"
#include "ct_events.hpp"
#include "ct_component.hpp"
#include "clap_helpers.hpp"
#include <travesty/events.h>
#include <cstdlib>

event_converter_v3_to_clap::event_converter_v3_to_clap(ct_component *comp)
    : m_cache{comp->m_cache->get_params()}
{
}

void event_converter_v3_to_clap::transfer()
{
    ct_events_buffer *out = m_out;
    const ct_caches::params_t *cache = m_cache;

    if (v3::param_changes *pcs = m_pcs) {
        int32_t nparams = pcs->m_vptr->i_changes.get_param_count(pcs);
        for (int32_t iparam = 0; iparam < nparams; ++iparam) {
            v3::param_value_queue *vq = (v3::param_value_queue *)pcs->m_vptr->i_changes.get_param_data(pcs, iparam);
            if (!vq)
                continue;
            int32_t npoints = vq->m_vptr->i_queue.get_point_count(vq);
            if (npoints < 1)
                continue;
            v3_param_id id = vq->m_vptr->i_queue.get_param_id(vq);
            for (int32_t ipoint = npoints; ipoint-- > 0; ) {
                int32_t raw_offset = 0;
                double value = 0;
                if (vq->m_vptr->i_queue.get_point(vq, ipoint, &raw_offset, &value) == V3_OK)
                    convert_parameter_change(id, raw_offset, value, cache, out);
            }
        }
    }

    if (v3::event_list *evs = m_evs) {
        int32_t nevents = evs->m_vptr->i_list.get_event_count(evs);
        for (int32_t ievent = 0; ievent < nevents; ++ievent) {
            v3_event event{};
            if (evs->m_vptr->i_list.get_event(evs, ievent, &event) == V3_OK)
                convert_event(&event, out);
        }
    }

    if (m_sort)
        out->sort_events_by_time(); //XXX could improve
}

uint32_t event_converter_v3_to_clap::fix_offset(int32_t offset)
{
    return (offset > 0) ? (uint32_t)offset : 0;
}

void event_converter_v3_to_clap::convert_event(const v3_event *event, ct_events_buffer *result)
{
    switch (event->type) {
    case V3_EVENT_NOTE_ON:
    {
        clap_event_note ce{};
        ce.header.size = sizeof(ce);
        ce.header.time = fix_offset(event->sample_offset);
        ce.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ce.header.type = CLAP_EVENT_NOTE_ON;
        ce.header.flags = (event->flags & V3_EVENT_IS_LIVE) ? CLAP_EVENT_IS_LIVE : 0;
        ce.note_id = event->note_on.note_id;
        ce.port_index = event->bus_index;
        ce.key = event->note_on.pitch;
        ce.channel = event->note_on.channel;
        ce.velocity = event->note_on.velocity;
        result->add(&ce.header);
    }
    break;

    case V3_EVENT_NOTE_OFF:
    {
        clap_event_note ce{};
        ce.header.size = sizeof(ce);
        ce.header.time = fix_offset(event->sample_offset);
        ce.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ce.header.type = CLAP_EVENT_NOTE_OFF;
        ce.header.flags = (event->flags & V3_EVENT_IS_LIVE) ? CLAP_EVENT_IS_LIVE : 0;
        ce.port_index = event->bus_index;
        ce.note_id = event->note_off.note_id;
        ce.key = event->note_off.pitch;
        ce.channel = event->note_off.channel;
        ce.velocity = event->note_off.velocity;
        result->add(&ce.header);
    }
    break;

    case V3_EVENT_NOTE_EXP_VALUE:
    {
        clap_event_note_expression ce{};
        ce.header.size = sizeof(ce);
        ce.header.time = fix_offset(event->sample_offset);
        ce.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ce.header.type = CLAP_EVENT_NOTE_EXPRESSION;
        ce.header.flags = (event->flags & V3_EVENT_IS_LIVE) ? CLAP_EVENT_IS_LIVE : 0;
        ce.note_id = event->note_exp_value.note_id;
        ce.port_index = event->bus_index;
        ce.key = -1; // don't have it
        ce.channel = -1;
        ce.value = event->note_exp_value.value;
        if (event->note_exp_value.type_id == 0) {
            ce.expression_id = CLAP_NOTE_EXPRESSION_VOLUME;
            ce.value = 4 * ce.value;
        }
        else if (event->note_exp_value.type_id == 1) {
            ce.expression_id = CLAP_NOTE_EXPRESSION_PAN;
        }
        else if (event->note_exp_value.type_id == 2) {
            ce.expression_id = CLAP_NOTE_EXPRESSION_TUNING;
            ce.value *= 240;
            ce.value -= 120;
        }
        else if (event->note_exp_value.type_id == 3) {
            ce.expression_id = CLAP_NOTE_EXPRESSION_VIBRATO;
        }
        else if (event->note_exp_value.type_id == 4) {
            ce.expression_id = CLAP_NOTE_EXPRESSION_EXPRESSION;
        }
        else if (event->note_exp_value.type_id == 5) {
            ce.expression_id = CLAP_NOTE_EXPRESSION_BRIGHTNESS;
        }
        else {
            break;
        }
        result->add(&ce.header);
    }
    break;
    }
}

void event_converter_v3_to_clap::convert_parameter_change(uint32_t id, int32_t offset, double value, const ct_caches::params_t *clap_params, ct_events_buffer *result)
{
    const clap_param_info *info = clap_params->get_param_by_id(id);
    if (!info)
        return;

    clap_event_param_value ce{};
    ce.header.size = sizeof(ce);
    ce.header.time = fix_offset(offset);
    ce.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ce.header.type = CLAP_EVENT_PARAM_VALUE;
    ce.param_id = id;
    ce.port_index = -1;
    ce.value = denormalize_parameter_value(info, value);
    result->add(&ce.header);
}

//------------------------------------------------------------------------------
event_converter_clap_to_v3::event_converter_clap_to_v3(ct_component *comp)
    : m_cache{comp->m_cache->get_params()}
{
    // reserve parameter memory
    m_queues.resize(m_cache->m_params.size());
}

void event_converter_clap_to_v3::transfer()
{
    const ct_events_buffer *in = m_in;
    v3::param_changes *pcs = m_pcs;
    v3::event_list *evs = m_evs;
    uint32_t count = in->count();

    if (pcs)
        std::memset(m_queues.data(), 0, m_queues.size() * sizeof(v3::param_value_queue *));

    for (uint32_t i = 0; i < count; ++i) {
        const clap_event_header *hdr = in->get(i);

        // TODO event ppq position (1/4 beats)
        auto event_ppq_position = []() -> double {
            return 0.0;
        };

        switch (hdr->type) {
        case CLAP_EVENT_NOTE_ON:
        {
            if (!evs)
                break;

            const clap_event_note *ce = (const clap_event_note *)hdr;

            v3_event event{};
            event.bus_index = ce->port_index;
            event.sample_offset = (int32_t)hdr->time;
            event.ppq_position = event_ppq_position();
            event.flags = (hdr->flags & CLAP_EVENT_IS_LIVE) ? V3_EVENT_IS_LIVE : 0;
            event.note_on.channel = ce->channel;
            event.note_on.pitch = ce->key;
            event.note_on.tuning = 0;
            event.note_on.velocity = (float)ce->velocity;
            event.note_on.length = 0;
            event.note_on.note_id = ce->note_id;
            evs->m_vptr->i_list.add_event(evs, &event);
        }
        break;

        case CLAP_EVENT_NOTE_OFF:
        {
            if (!evs)
                break;

            const clap_event_note *ce = (const clap_event_note *)hdr;

            v3_event event{};
            event.bus_index = ce->port_index;
            event.sample_offset = (int32_t)hdr->time;
            event.ppq_position = event_ppq_position();
            event.flags = (hdr->flags & CLAP_EVENT_IS_LIVE) ? V3_EVENT_IS_LIVE : 0;
            event.note_off.channel = ce->channel;
            event.note_off.pitch = ce->key;
            event.note_off.tuning = 0;
            event.note_off.velocity = (float)ce->velocity;
            event.note_off.note_id = ce->note_id;
            evs->m_vptr->i_list.add_event(evs, &event);
        }
        break;

        case CLAP_EVENT_NOTE_EXPRESSION:
        {
            if (!evs)
                break;

            const clap_event_note_expression *ce = (const clap_event_note_expression *)hdr;

            v3_event event{};
            event.bus_index = ce->port_index;
            event.sample_offset = (int32_t)hdr->time;
            event.ppq_position = event_ppq_position();
            event.flags = (hdr->flags & CLAP_EVENT_IS_LIVE) ? V3_EVENT_IS_LIVE : 0;
            event.note_exp_value.note_id = ce->note_id;
            event.note_exp_value.value = ce->value;
            switch (ce->expression_id) {
            case CLAP_NOTE_EXPRESSION_VOLUME:
                event.note_exp_value.type_id = 0;
                event.note_exp_value.value /= 4;
                break;
            case CLAP_NOTE_EXPRESSION_PAN:
                event.note_exp_value.type_id = 1;
                break;
            case CLAP_NOTE_EXPRESSION_TUNING:
                event.note_exp_value.type_id = 2;
                event.note_exp_value.value += 120;
                event.note_exp_value.value /= 240;
                break;
            case CLAP_NOTE_EXPRESSION_VIBRATO:
                event.note_exp_value.type_id = 3;
                break;
            case CLAP_NOTE_EXPRESSION_EXPRESSION:
                event.note_exp_value.type_id = 4;
                break;
            case CLAP_NOTE_EXPRESSION_BRIGHTNESS:
                event.note_exp_value.type_id = 5;
                break;
            default:
                break;
            }
            evs->m_vptr->i_list.add_event(evs, &event);
        }
        break;

        case CLAP_EVENT_PARAM_VALUE:
        {
            if (!pcs)
                break;

            const clap_event_param_value *ce = (const clap_event_param_value *)hdr;
            clap_id id = ce->param_id;

            const uint32_t *param_indexp = m_cache->m_param_idx_by_id.find(id);
            if (!param_indexp)
                break;

            const uint32_t param_index = *param_indexp;
            v3::param_value_queue *queue = m_queues[param_index];
            v3::param_value_queue *invalid_queue = (v3::param_value_queue *)0x1;

            if (!queue) {
                int32_t queue_index = 0;
                queue = (v3::param_value_queue *)pcs->m_vptr->i_changes.add_param_data(pcs, &id, &queue_index);
                if (!queue)
                    queue = invalid_queue;
                m_queues[param_index] = queue;
            }

            if (queue == invalid_queue)
                break;

            double normalized = normalize_parameter_value(&m_cache->m_params[param_index], ce->value);
            int32_t value_index = 0;
            queue->m_vptr->i_queue.add_point(queue, (int32_t)hdr->time, normalized, &value_index);
        }
        break;
        }
    }
}

//------------------------------------------------------------------------------
void convert_v3_transport_to_clap(const v3_process_context *ctx, clap_event_transport *transport)
{
        //XXX do no provide the seconds timeline
        // context has the position in seconds and beats
        // but the loop range is in beats only

        auto beats_time = [](double beats) -> clap_beattime {
            return (clap_beattime)(0.5 + CLAP_BEATTIME_FACTOR * beats);
        };
        //auto seconds_time = [](double seconds) -> clap_sectime {
        //    return (clap_sectime)(0.5 + CLAP_SECTIME_FACTOR * seconds);
        //};

        ///
        transport->flags &= ~CLAP_TRANSPORT_IS_PLAYING;
        if (ctx->state & V3_PROCESS_CTX_PLAYING)
            transport->flags |= CLAP_TRANSPORT_IS_PLAYING;

        transport->flags &= ~CLAP_TRANSPORT_IS_RECORDING;
        if (ctx->state & V3_PROCESS_CTX_RECORDING)
            transport->flags |= CLAP_TRANSPORT_IS_RECORDING;

        transport->flags &= ~CLAP_TRANSPORT_IS_LOOP_ACTIVE;
        if (ctx->state & V3_PROCESS_CTX_CYCLE_ACTIVE)
            transport->flags |= CLAP_TRANSPORT_IS_LOOP_ACTIVE;

        if (ctx->state & V3_PROCESS_CTX_PROJECT_TIME_VALID) {
            transport->song_pos_beats = beats_time(ctx->project_time_quarters);
            //transport->song_pos_seconds = seconds_time(ctx->project_time_in_samples / ctx->sample_rate);
            transport->flags |= CLAP_TRANSPORT_HAS_BEATS_TIMELINE;
            //transport->flags |= CLAP_TRANSPORT_HAS_SECONDS_TIMELINE;
        }

        if (ctx->state & V3_PROCESS_CTX_CYCLE_VALID) {
            transport->loop_start_beats = beats_time(ctx->cycle_start_quarters);
            transport->loop_end_beats = beats_time(ctx->cycle_end_quarters);
            //transport->loop_start_seconds = ;
            //transport->loop_end_seconds = ;
        }

        if (ctx->state & V3_PROCESS_CTX_TEMPO_VALID) {
            transport->tempo = ctx->bpm;
            transport->flags |= CLAP_TRANSPORT_HAS_TEMPO;
        }

        if (ctx->state & V3_PROCESS_CTX_TIME_SIG_VALID) {
            transport->tsig_num = (int16_t)ctx->time_sig_numerator;
            transport->tsig_denom = (int16_t)ctx->time_sig_denom;
            transport->flags |= CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
        }

        if (ctx->state & V3_PROCESS_CTX_BAR_POSITION_VALID) {
            transport->bar_start = beats_time(ctx->bar_position_quarters);
#if 0
            //XXX bar number not given, should I calculate one?
            if (transport->flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE)
                transport->bar_number = (int32_t)(ctx->bar_position_quarters / transport->tsig_num);
#endif
        }
}
