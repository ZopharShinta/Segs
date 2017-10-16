#include "Powers.h"

#include <ace/Log_Msg.h>

void Power::serializeto(BitStream &tgt) const
{
    tgt.StoreBits(4,entry_type);
    switch(entry_type)
    {
    case 1:
        tgt.StoreBits(32,unkn1); //powersetIdx
        tgt.StoreBits(32,unkn2);
        break;
    case 2:
        tgt.StorePackedBits(3,unkn1);
        tgt.StorePackedBits(3,unkn2);
        break;
    case 6:
    case 12:
        tgt.StoreString(sunkn1);
        tgt.StoreString(sunkn2);
        tgt.StoreString(sunkn3);
        break;
    case 0:
        break;
    default:
        ACE_DEBUG((LM_WARNING,ACE_TEXT("(%P|%t) Unknown tray entry type %d\n"),entry_type));
    }
}

void Power::serializefrom(BitStream &src)
{
    entry_type = src.GetBits(4);
    switch(entry_type)
    {
    case 1:
        unkn1 = src.GetBits(32);
        unkn2 = src.GetBits(32);
        break;
    case 2:
        unkn1 = src.GetPackedBits(3);
        unkn2 = src.GetPackedBits(3);
        break;
    case 6:
    case 12:
        src.GetString(sunkn1);
        src.GetString(sunkn2);
        src.GetString(sunkn3);
        break;
    case 0:
        break;
    default:
        ACE_DEBUG((LM_WARNING,ACE_TEXT("(%P|%t) Unknown tray entry type %d\n"),entry_type));
    }
}

void Power::Dump()
{
    switch(entry_type)
    {
    case 1: case 2:
        ACE_DEBUG ((LM_DEBUG,ACE_TEXT ("[(0x%x,0x%x)]"),unkn1,unkn2));
        break;
    case 6: case 12:
        ACE_DEBUG ((LM_DEBUG,ACE_TEXT ("[(%s,%s,%s)]"),qPrintable(sunkn1),qPrintable(sunkn2),qPrintable(sunkn3)));
        break;
    case 0:
        break;
    default:
        ACE_DEBUG((LM_WARNING,ACE_TEXT("(%P|%t) Unknown tray entry type %d\n"),entry_type));
    }

}

void PowerTrayGroup::serializeto(BitStream &tgt) const
{
    tgt.StoreBits(32,primary_tray_idx);
    tgt.StoreBits(32,secondary_tray_idx);
    for(int bar_num=0; bar_num<9; bar_num++)
        m_trays[bar_num].serializeto(tgt);
    bool m_c = false;
    tgt.StoreBits(1,m_c);
    if(m_c) // selected ???
    {
        tgt.StoreBits(32,m_power_rel1); // array of powersets index
        tgt.StoreBits(32,m_power_rel2); // array of powers idx ?
    }
}

void PowerTrayGroup::serializefrom(BitStream &src)
{
    primary_tray_idx = src.GetBits(32);
    secondary_tray_idx = src.GetBits(32);
    for(PowerTray &tray : m_trays)
    {
        tray.serializefrom(src);
    }
    m_c = src.GetBits(1);
    if(m_c)
    {
        m_power_rel1= src.GetBits(32);
        m_power_rel2= src.GetBits(32);
    }
}

void PowerTrayGroup::dump()
{
    ACE_DEBUG ((LM_DEBUG,ACE_TEXT ("%I    unkn1: 0x%08x\n"),primary_tray_idx));
    ACE_DEBUG ((LM_DEBUG,ACE_TEXT ("%I    unkn2: 0x%08x\n"),secondary_tray_idx));
    for(int bar_num=0; bar_num<9; bar_num++)
    {
        if(m_trays[bar_num].setPowers()==0)
            continue;
        ACE_DEBUG ((LM_DEBUG,ACE_TEXT ("%I    Tray %d ***"),bar_num));
        m_trays[bar_num].Dump();
        ACE_DEBUG ((LM_DEBUG,ACE_TEXT ("***\n")));
    }
    ACE_DEBUG ((LM_DEBUG,ACE_TEXT ("%I    m_c %d\n"),m_c));
    if(m_c)
    {
        ACE_DEBUG ((LM_DEBUG,ACE_TEXT ("%I    m_power_rel1 0x%08x\n"),m_power_rel1));
        ACE_DEBUG ((LM_DEBUG,ACE_TEXT ("%I    m_power_rel2 0x%08x\n"),m_power_rel2));
    }
}

Power *PowerTray::getPower(size_t idx)
{
    if(idx<10)
        return &m_powers[idx];
    return nullptr;
}

int PowerTray::setPowers()
{
    int res=0;
    for(const Power &pow : m_powers)
    {
        res += (pow.entry_type!=0);
    }
    return res;
}

void PowerTray::serializefrom(BitStream &src)
{
    for(Power &pow : m_powers)
        pow.serializefrom(src);
}

void PowerTray::serializeto(BitStream &tgt) const
{
    for(const Power &pow : m_powers)
        pow.serializeto(tgt);
}

void PowerTray::Dump()
{
    for(Power &pow : m_powers)
    {
        pow.Dump();
    }

}

void PowerPool_Info::serializefrom(BitStream &src)
{
    id[0] = src.GetPackedBits(3);
    id[1] = src.GetPackedBits(3);
    id[2] = src.GetPackedBits(3);
}

void PowerPool_Info::serializeto(BitStream &src) const
{
    src.StorePackedBits(3,id[0]);
    src.StorePackedBits(3,id[1]);
    src.StorePackedBits(3,id[2]);
}
