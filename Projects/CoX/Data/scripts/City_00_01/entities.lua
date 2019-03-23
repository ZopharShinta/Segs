-- OUTBREAK

local spawnOnce = false

entity_interact = function(id, location)

  return ""
end

-- Called after MOTD for now.
function player_connected(id)
    --Id is player entity Id
    printDebug('player_connected Id: ' .. tostring(id))

    Tasks.UpdateTasksForZone('OutBreak')
    Contacts.SpawnContacts('OutBreak')

    if spawnOnce == false then
        spinSpawners()
        spinPersists()
        --Civilians and cars don't spawn in Outbreak, though they could ...
        --spinCivilians()
        --spinCars()
        RandomSpawn(65)
        spawnOnce = true
    end

    return  ''
end

function npc_added(id)
    printDebug('npc_added Id: ' .. tostring(id))
    Contacts.SpawnedContact(id)
    -- Spawn next contact
    Contacts.SpawnContacts('OutBreak')

    return ''
end

set_target = function(id)
    print("target id " .. tostring(id))
    Character.faceEntity(client.m_ent, id)
    return ""
end

dialog_button = function(id) -- Will be called if no callback is set
    printDebug("No Callback set! ButtonId: " .. tostring(id))

    return ""
end

contact_call = function(contactIndex)
    printDebug("Contact Call. contactIndex: " .. tostring(contactIndex))

    return ""
end

revive_ok = function(id)
    printDebug("revive Ok. Entity: " .. tostring(id))
    Character.respawn(client, 'Gurney'); -- Hospital
    Player.Revive(0);

    return "ok"
end



