from StateSystem.EventListener import EventListener
from StateSystem.CreatureCapacity import CreatureCapacity
from StateSystem.Artifact import HolyLightArtifact, SalamanderShieldArtifact
from StateSystem.UnitData import CREATURE_CAPACITY_LEVEL_UP_TURN

class Player:
    def __init__(self,camp,mana,state_system):
        self.camp = camp
        self.artifact_list = []
        self.creature_capacity_list = []
        self.newly_summoned_id_list = []
        self.max_mana = mana
        self.mana = mana
        self.state_system = state_system
        self.event_listener_list = []
        self.score = 0

        self.add_event_listener(RefreshListener())
        self.add_event_listener(IntoCoolDownListener())
        self.add_event_listener(SummonListener())
        self.add_event_listener(ActivateArtifactListener())
        self.add_event_listener(ScoreListener())
        self.add_event_listener(CreatureCapacityLevelUpListener())

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
            [artifact.parse() for artifact in self.artifact_list], # artifact
            self.mana,
            self.max_mana,
            [capacity.parse() for capacity in self.creature_capacity_list],
            self.newly_summoned_id_list
        ]

class RefreshListener(EventListener):
    def deal_event(self,event):
        if event.name == "Refresh" and event.parameter_dict["camp"] == self.host.camp:
            if self.host.max_mana < 12:
                # on the 4kth turn camp=1 player
                # on the 4k+1st turn camp=0 player
                if event.parameter_dict["turn"] % 4 == 1 - self.host.camp:
                    self.host.max_mana += 1
            self.host.mana = self.host.max_mana
            for capacity in self.host.creature_capacity_list:
                capacity.cool_down()
            for artifact in self.host.artifact_list:
                artifact.cool_down()
            self.host.newly_summoned_id_list = []

class IntoCoolDownListener(EventListener):
    '''
        A creature dies
    '''
    def deal_event(self,event):
        if event.name == "Death":
            source = event.parameter_dict["source"]
            if source.camp == self.host.camp:
                for capacity in self.host.creature_capacity_list:
                    if capacity.type == source.type:
                        capacity.new_cool_down(source.level)
                        # print("Player {}'s creature {}(ID: {}) starts cooling down for {} turns.".format(
                        #     self.host.camp,
                        #     source.name,
                        #     source.id,
                        #     source.cool_down
                        # ))

class SummonListener(EventListener):
    def deal_event(self,event):
        if event.name == "Spawn":
            source = event.parameter_dict["source"]
            if source.camp == self.host.camp:
                self.host.mana -= source.cost
                for capacity in self.host.creature_capacity_list:
                    if capacity.type == source.type:
                        capacity.summon()
                self.host.newly_summoned_id_list.append(source.id)

class ActivateArtifactListener(EventListener):
    def deal_event(self,event):
        if event.name == "ActivateArtifact":
            if event.parameter_dict["camp"] == self.host.camp:
                for artifact in self.host.artifact_list:
                    if artifact.name == event.parameter_dict["name"]:
                        artifact.activate(event.parameter_dict["target"])
                        # print("Player {} activate {} !!!".format(self.host.camp,artifact.name))
                        return

class ScoreListener(EventListener):
    def deal_event(self,event):
        if event.name == "MiracleHurt" and event.parameter_dict["source"].camp != self.host.camp:
            self.host.score += event.parameter_dict["hp_loss"] * 1000
        if event.name == "Death" and event.parameter_dict["source"].camp != self.host.camp:
            self.host.score += event.parameter_dict["source"].level
            
class CreatureCapacityLevelUpListener(EventListener):
    def deal_event(self,event):
        if event.name == "Refresh" and event.parameter_dict["turn"] in CREATURE_CAPACITY_LEVEL_UP_TURN:
            for item in self.host.creature_capacity_list:
                item.duplicate_level_up()