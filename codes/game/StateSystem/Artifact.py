from Geometry import calculator
from StateSystem.Event import Event
from StateSystem.Buff import Buff
from StateSystem.EventListener import EventListener
from StateSystem.Unit import Unit
from StateSystem.UnitData import ARTIFACT_NAME_PARSED,ARTIFACT_STATE_PARSED,ARTIFACT_TARGET_PARSED,ARTIFACTS

ARTIFACT_ID = 0

def gen_artifact_by_name(name,camp,state_system):
    if name == "HolyLight":
        return HolyLightArtifact(camp,state_system)
    elif name == "SalamanderShield":
        return SalamanderShieldArtifact(camp,state_system)
    elif name == "InfernoFlame":
        return InfernoFlameArtifact(camp,state_system)
    elif name == "WindBlessing":
        return WindBlessingArtifact(camp,state_system)
    else:
        return None

class Artifact:
    def __init__(self,camp,name,state_system):
        global ARTIFACT_ID
        self.id = ARTIFACT_ID
        ARTIFACT_ID += 1
        self.state_system = state_system
        self.event_listener_list = []
        self.cost = ARTIFACTS[name]["cost"]
        self.max_cool_down = ARTIFACTS[name]["cool_down"]
        self.cool_down_time = 0
        self.state = "Ready"
        self.camp = camp
        self.name = name
        self.last_used_pos = (-1,-1,-1)
        self.target_type = ARTIFACTS[name]["target_type"]
    
    def add_event_listener(self,listener):
        listener.host = self
        self.event_listener_list.append(listener)

    def deal_event(self,event):
        for listener in self.event_listener_list:
            listener.deal_event(event)

    def emit(self,event):
        self.state_system.emit(event)

    def parse(self):
        return [
            self.id,
            ARTIFACT_NAME_PARSED[self.name],
            self.cost,
            self.max_cool_down,
            self.cool_down_time,
            ARTIFACT_STATE_PARSED[self.state],
            ARTIFACT_TARGET_PARSED[self.target_type],
            list(self.last_used_pos)
        ]

    def activate(self,target):
        self.state = "In Use"
        if(self.target_type == "Unit"):
            self.last_used_pos = target.pos
        else:
            self.last_used_pos = target
        self.state_system.get_player_by_id(self.camp).mana -= self.cost
        self.effect(target)
        
    def effect(self,target):
        pass

    def recycle(self):
        self.state = "Cooling Down"
        self.cool_down_time = self.max_cool_down

    def cool_down(self):
        if self.state == "Cooling Down":
            if self.cool_down_time > 0:
                self.cool_down_time -= 1
            if self.cool_down_time == 0:
                self.state = "Ready"

class HolyLightArtifact(Artifact):
    def __init__(self,camp,state_system):
        Artifact.__init__(self,camp,"HolyLight",state_system)

    def effect(self,target):
        for unit in self.state_system.map.unit_list:
            if calculator.cube_distance(unit.pos,target) <= ARTIFACTS["HolyLight"]["affect_range"] \
                and unit.camp == self.camp:
                self.emit(Event("Heal",{
                    "source": self,
                    "target": unit,
                    "heal": unit.max_hp
                },-1))
                new_buff = HolyLightAtkBuff(self.state_system)
                new_buff.add_on(unit)
        self.recycle()
        
class RemoveOnEndTurnListener(EventListener):
    def deal_event(self,event):
        if event.name == "TurnEnd":
            self.host.effect_rounds -= 1
            if self.host.effect_rounds == 0:
                self.host.delete()

class HolyLightAtkBuff(Buff):
    def __init__(self,state_system):
        Buff.__init__(self,state_system)
        self.add_event_listener(RemoveOnEndTurnListener())
        self.type = "HolyLightAtkBuff"
        self.effect_rounds = ARTIFACTS["HolyLight"]["effect_rounds"]

    def buff(self):
        self.host.atk += ARTIFACTS["HolyLight"]["atk_up"]

    def debuff(self):
        self.host.atk -= ARTIFACTS["HolyLight"]["atk_up"]

class SalamanderShieldArtifact(Artifact):
    def __init__(self,camp,state_system):
        Artifact.__init__(self,camp,"SalamanderShield",state_system)

    def effect(self,target):
        new_buff = SalamanderShieldBuff(self.state_system, self)
        new_buff.add_on(target)

class SalamanderShieldRefreshListener(EventListener):
    def deal_event(self,event):
        if event.name == "TurnStart" and self.host.host.state_system.current_player_id == self.host.host.camp \
            and not self.host.host.holy_shield:
            self.host.emit(Event("BuffAdd",{
                "source": self.host.host,
                "type": "HolyShield"
            },-1))

class SalamanderShieldDeathRecycleListener(EventListener):
    def deal_event(self,event):
        if event.name == "Death" and event.parameter_dict["source"] == self.host.host:
            self.host.artifact_host.recycle()
            self.host.delete()

class SalamanderShieldBuff(Buff):
    def __init__(self,state_system,artifact_host):
        self.artifact_host = artifact_host
        Buff.__init__(self,state_system)
        self.add_event_listener(SalamanderShieldRefreshListener())
        self.add_event_listener(SalamanderShieldDeathRecycleListener())
        self.type = "SalamanderShieldBuff"

    def buff(self):
        self.host.max_hp += ARTIFACTS["SalamanderShield"]["hp_up"]
        self.host.hp += ARTIFACTS["SalamanderShield"]["hp_up"]
        if not self.host.holy_shield:
            self.state_system.emit(Event("BuffAdd",{
                    "source": self.host,
                    "type": "HolyShield"
                },1))

    def debuff(self):
        self.host.max_hp -= ARTIFACTS["SalamanderShield"]["hp_up"]
        self.host.hp = min(self.host.hp, self.host.max_hp)

class InfernoFlameArtifact(Artifact):
    def __init__(self,camp,state_system):
        Artifact.__init__(self,camp,"InfernoFlame",state_system)

    def effect(self,target):
        for unit in self.state_system.map.unit_list:
            if calculator.cube_distance(unit.pos,target) <= ARTIFACTS[self.name]["affect_range"] \
                and unit.camp != self.camp:
                self.emit(Event("Damage",{
                    "source": self,
                    "target": unit,
                    "damage": ARTIFACTS[self.name]["damage"],
                    "type": "InfernoFlameActivate"
                },-3))
        self.emit(Event("Summon",{
            "type": ARTIFACTS[self.name]["summon"],
            "level": 1,
            "pos": target,
            "camp": self.camp,
            "artifact_host": self
        }))
        self.emit(Event("CheckDeath",priority=4))
        
class Inferno(Unit):
    def __init__(self,camp,level,pos,state_system,artifact_host):
        name = "Inferno"
        self.artifact_host = artifact_host
        Unit.__init__(
            self,
            camp,
            name,
            level, # Only a single level
            pos,
            state_system
        )

        self.add_event_listener(InfernoRecycleListener())

class InfernoRecycleListener(EventListener):
    def deal_event(self,event):
        if event.name == "Death" and event.parameter_dict["source"] == self.host:
            self.host.artifact_host.recycle()

class WindBlessingArtifact(Artifact):
    def __init__(self,camp,state_system):
        Artifact.__init__(self,camp,"WindBlessing",state_system)

    def effect(self,target):
        for unit in self.state_system.map.unit_list:
            if calculator.cube_distance(unit.pos,target) <= ARTIFACTS["WindBlessing"]["affect_range"] \
                and unit.camp == self.camp:
                unit.can_atk = True
                unit.can_move = True
        self.recycle()