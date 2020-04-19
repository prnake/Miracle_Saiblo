from StateSystem.EventListener import EventListener
from StateSystem.Event import Event

class Miracle:
    def __init__(self,camp,hp,pos,summon_pos_list,state_system):
        self.name = "Miracle (belongs to Player {})".format(camp)
        self.type = "Miracle"
        self.id = camp
        self.max_hp = hp
        self.hp = hp
        self.camp = camp
        self.pos = pos
        self.summon_pos_list = summon_pos_list
        self.state_system = state_system
        self.event_listener_list = []

        self.add_event_listener(MiracleDamageListener())

    def add_event_listener(self,listener):
        listener.host = self
        self.event_listener_list.append(listener)

    def deal_event(self,event):
        for listener in self.event_listener_list:
            listener.deal_event(event)

    def emit(self,event):
        self.state_system.emit(event)
    
    def parse(self):
        return self.hp

class MiracleDamageListener(EventListener):
    def deal_event(self,event):
        if event.name == "Damage":
            if event.parameter_dict["target"] == self.host:
                hp_loss = min(self.host.hp,event.parameter_dict["damage"])
                self.host.hp -= event.parameter_dict["damage"]
                self.host.emit(Event("MiracleHurt",{
                    "source": self.host,
                    "hp_loss": hp_loss
                }))