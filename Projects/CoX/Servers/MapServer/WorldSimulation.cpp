/*
 * SEGS - Super Entity Game Server
 * http://www.segs.io/
 * Copyright (c) 2006 - 2018 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License. See LICENSE.md for details.
 */

/*!
 * @addtogroup MapServer Projects/CoX/Servers/MapServer
 * @{
 */

#include "WorldSimulation.h"

#include "Common/Servers/Database.h"
#include "DataHelpers.h"
#include "Messages/Map/GameCommandList.h"
#include "GameData/Character.h"
#include "GameData/CharacterHelpers.h"
#include <glm/gtx/vector_query.hpp>
void World::update(const ACE_Time_Value &tick_timer)
{
    ACE_Time_Value delta;
    if(prev_tick_time==ACE_Time_Value::zero)
    {
        delta = ACE_Time_Value(0,33*1000);
    }
    else
        delta = tick_timer - prev_tick_time;
    m_time_of_day+= 4.8f*((float(delta.msec())/1000.0f)/(60.0f*60)); // 1 sec of real time is 48s of ingame time
    if(m_time_of_day>=24.0f)
        m_time_of_day-=24.0f;
    sim_frame_time = delta.msec()/1000.0f;
    accumulated_time += sim_frame_time;
    prev_tick_time = tick_timer;
    ACE_Guard<ACE_Thread_Mutex> guard_buffer(ref_ent_mager.getEntitiesMutex());

    for(Entity * e : ref_ent_mager.m_live_entlist)
        updateEntity(e,delta);
}

void World::physicsStep(Entity *e,uint32_t msec)
{
    if(glm::length2(e->m_states.current()->m_pos_delta))
    {
        setVelocity(*e);
        //e->m_motion_state.m_velocity = za*e->m_states.current()->m_pos_delta;

        // todo: take into account time between updates
        glm::mat3 za = static_cast<glm::mat3>(e->m_direction); // quat to mat4x4 conversion
        //float vel_scale = e->inp_state.m_input_vel_scale/255.0f;
        e->m_entity_data.m_pos += ((za*e->m_states.current()->m_pos_delta)*float(msec))/40.0f; // formerly 50.0f
    }
}

float animateValue(float v,float start,float target,float length,float dT)
{
    float range=target-start;
    float current_pos = (v-start)/range;
    float accumulated_time = length*current_pos;
    accumulated_time = std::min(length,accumulated_time+dT);
    float res = start + (accumulated_time/length) * range;
    return res;
}

void World::effectsStep(Entity *e,uint32_t msec)
{
    if(e->m_is_fading)
    {
        float target=0.0f;
        float start=1.0f;
        if(e->m_fading_direction!=FadeDirection::In)
        { // must be fading out, so our target is 100% transparency.
            target = 1;
            start = 0;
        }
        e->translucency = animateValue(e->translucency,start,target,m_player_fade_in,float(msec)/50.0f);
        if(std::abs(e->translucency-target)<std::numeric_limits<float>::epsilon())
            e->m_is_fading = false;
    }
}

void World::checkPowerTimers(Entity *e, uint32_t msec)
{

    // for now we only run this on players
    if(e->m_type != EntType::PLAYER)
        return;
    // Activation Timers -- queue FIFO
    if(e->m_queued_powers.size() > 0)
    {
        QueuedPowers &qpow = e->m_queued_powers.front();
        if (e->m_is_activating == false)
        {
            if (checkPowerRecharge(*e, qpow.m_pow_idxs.m_pset_vec_idx, qpow.m_pow_idxs.m_pow_vec_idx)
                    && checkPowerRange(*e, qpow.m_tgt_idx, qpow.m_pow_idxs.m_pset_vec_idx, qpow.m_pow_idxs.m_pow_vec_idx))
            {
                   e->m_is_activating = true;       //queued power can move forward to an active power
                   checkMovement(*e);               //stop movement while casting
            }
        }
        else if (qpow.m_activation_state)           //sometimes powers get turned off before they can be removed from the queue
        {
            qpow.m_time_to_activate -= (float(msec)/1000);

            if(qpow.m_time_to_activate < 0 && qpow.m_activation_state)
            {
                CharacterPower * ppower = getOwnedPowerByVecIdx(*e, qpow.m_pow_idxs.m_pset_vec_idx, qpow.m_pow_idxs.m_pow_vec_idx);
                if (ppower->getPowerTemplate().Type == PowerType::Toggle)
                {
                    e->m_auto_powers.push_back(qpow);
                    e->m_queued_powers.dequeue();
                }
                else
                {
                    doPower(*e, qpow);
                    qpow.m_activation_state = false;                //this will turn off the activation ring and then dequeue
                    qpow.m_active_state_change = true;
                    e->m_char->m_char_data.m_has_updated_powers = true;
                }
                e->m_is_activating = false;
                checkMovement(*e);                                  //frees up movement, unless held by other means
            }
            else
            {
                QString from_msg = "charging!";                     //so players know they are activating a power
                sendFloatingInfo(*e->m_client, from_msg, FloatingInfoStyle::FloatingInfo_Info, 0.0);
            }
        }
    }
    else
    {           // only activate the default power if there is no other power in the activation queue
        PowerTrayGroup &trays =  e->m_char->m_char_data.m_trays;
        if(trays.m_has_default_power)
        {
            if (checkPowerRecharge(*e, trays.m_default_pset_idx, trays.m_default_pow_idx))
                usePower(*e, trays.m_default_pset_idx, trays.m_default_pow_idx, getTargetIdx(*e));
        }
    }
    // Recharging Timers -- iterate through and remove finished timers
    for(auto rpow_idx = e->m_recharging_powers.begin(); rpow_idx != e->m_recharging_powers.end(); /*rpow_idx updated inside loop*/ )
    {
        rpow_idx->m_recharge_time -= (float(msec)/1000);

        if(rpow_idx->m_recharge_time <= 0)
        {
            rpow_idx = e->m_recharging_powers.erase(rpow_idx);
            e->m_char->m_char_data.m_has_updated_powers = true;
        }
        else
            ++rpow_idx;
    }

    // Auto and Toggle Power Activation Timers
    for(auto &rpow_idx : e->m_auto_powers)
    {
        CharacterPower * ppower = getOwnedPowerByVecIdx(*e, rpow_idx.m_pow_idxs.m_pset_vec_idx, rpow_idx.m_pow_idxs.m_pow_vec_idx);
        const Power_Data powtpl = ppower->getPowerTemplate();

        if (//(powtpl.Type == PowerType::Toggle && (
                 (getEnd(*e->m_char) < powtpl.EnduranceCost)
            || (isPlayerDead(e)) || (isPlayerDead(getEntity(e->m_client, rpow_idx.m_tgt_idx)))
            || !checkPowerRange(*e, rpow_idx.m_tgt_idx, rpow_idx.m_pow_idxs.m_pset_vec_idx, rpow_idx.m_pow_idxs.m_pow_vec_idx))//))

        {
            rpow_idx.m_activation_state = false;
            rpow_idx.m_active_state_change = true;
            e->m_char->m_char_data.m_has_updated_powers = true;         // dequeued in messagehelper

        }
        if(rpow_idx.m_time_to_activate   <= 0)
        {
            doPower(*e, rpow_idx);//qWarning() << rpow_idx.m_time_to_activate;
            rpow_idx.m_time_to_activate += rpow_idx.m_activate_period;
        }
        else
            rpow_idx.m_time_to_activate   -= (float(msec)/1000);
    }
    // Buffs
    for(auto thisbuff = e->m_buffs.begin(); thisbuff != e->m_buffs.end(); /*thisbuff updated inside loop*/)
    {
        if(thisbuff->m_duration <= 0 || isPlayerDead(e))
        {
            for (uint i =0; i<thisbuff->m_value_name.size();i++)        //there can be multiple values for one buff
            {
                modifyAttrib(*e, thisbuff->m_value_name[i], -thisbuff->m_value[i]);
            }
            thisbuff = e->m_buffs.erase(thisbuff);
        }
        else
        {
            thisbuff->m_duration -= (float(msec)/1000);                 // activate period is in minutes
            ++thisbuff;
        }
    }
}
bool World::isPlayerDead(Entity *e)
{
    if(e->m_type == EntType::PLAYER
            && getHP(*e->m_char) == 0.0f)
    {
        setStateMode(*e, ClientStates::DEAD);
        checkMovement(*e);
        return true;
    }

    return false;
}

void World::regenHealthEnd(Entity *e, uint32_t msec)
{
    // for now on Players only
    if(e->m_type == EntType::PLAYER)
    {
        float hp = getHP(*e->m_char);
        float end = getEnd(*e->m_char);

        float regeneration = getMaxHP(*e->m_char) * (e->m_char->m_char_data.m_current_attribs.m_Regeneration/20.0f * float(msec)/1000/12);
        float recovery = getMaxEnd(*e->m_char) * (e->m_char->m_char_data.m_current_attribs.m_Recovery/15.0f * float(msec)/1000/12);

        if(hp < getMaxHP(*e->m_char))
            setHP(*e->m_char, hp + regeneration);
        if(end < getMaxEnd(*e->m_char))
            setEnd(*e->m_char, end + recovery);
    }
}

void World::updateEntity(Entity *e, const ACE_Time_Value &dT)
{
    physicsStep(e, dT.msec());
    effectsStep(e, dT.msec());
    checkPowerTimers(e, dT.msec());

    // TODO: Issue #555 needs to handle team cleanup properly
    // and we need to remove the following
    if(e->m_team != nullptr)
    {
        if(e->m_team->m_team_members.size() <= 1)
        {
            qWarning() << "Team cleanup being handled in updateEntity, but we need to move this to TeamHandler";
            e->m_has_team = false;
            e->m_team = nullptr;
        }
    }

    // check death, set clienstate if dead, and
    // if alive, recover endurance and health
    if(!isPlayerDead(e))
        regenHealthEnd(e, dT.msec());

    if(e->m_is_logging_out)
    {
        e->m_time_till_logout -= dT.msec();
        if(e->m_time_till_logout<0)
            e->m_time_till_logout=0;
    }
}

//! @}
