from StateSystem.EventHeap import EventHeap
from StateSystem.Event import Event
from StateSystem.EventListener import EventListener
from StateSystem.Unit import *
from StateSystem.Map import *
from StateSystem.Player import Player
from StateSystem.Miracle import Miracle
from StateSystem.Obstacle import *
from StateSystem.Barrack import *
from StateSystem.CreatureCapacity import *
from StateSystem.Artifact import gen_artifact_by_name,Inferno

class StateSystem:
    def __init__(self):
        self.map = Map()
        self.event_heap = EventHeap()
        self.player_list = [Player(0,1,self),Player(1,2,self)]
        self.turn_count = 1
        self.current_player_id = 0
        self.map.miracle_list = [
            Miracle(0,30,(-7,7,0),[
                (-8,6,2),
                (-7,6,1),
                (-6,6,0),
                (-6,7,-1),
                (-6,8,-2)
            ],self),
            Miracle(1,30,(7,-7,0),[
                (8,-6,-2),
                (7,-6,-1),
                (6,-6,0),
                (6,-7,1),
                (6,-8,2)
            ],self)
        ]
        self.map.obstacle_list = [Obstacle("Abyss",ob_pos) for ob_pos in ABYSS_INIT_LIST]
        self.map.obstacle_list += [Obstacle("Miracle",(-7,7,0)), Obstacle("Miracle",(7,-7,0))]
        self.map.barrack_list = [Barrack(br[0],br[1],br[2]) for br in BARRACK_INIT_LIST]
        self.event_listener_list = []

        self.add_event_listener(SummonListener())
        self.add_event_listener(CheckDeathListener())
        self.add_event_listener(CheckBarrackListener())
        self.add_event_listener(TurnStartListener())
        self.add_event_listener(ChangeCurrentPlayerListener())
        self.add_event_listener(GameStartListener())

    def add_event_listener(self,listener):
        listener.host = self
        self.event_listener_list.append(listener)

    def deal_event(self,event):
        for listener in self.event_listener_list:
            listener.deal_event(event)

    def emit(self,event):
        self.event_heap.append(event)

    def start_event_processing(self):
        while self.event_heap.len():
            current_event = self.event_heap.pop()
            for player in self.player_list:
                player.deal_event(current_event)
            for unit in self.map.unit_list:
                unit.deal_event(current_event)
            for miracle in self.map.miracle_list:
                miracle.deal_event(current_event)
            self.deal_event(current_event)
        # Check Death
        new_unit_list = []
        for unit in self.map.unit_list:
            if not unit.death_flag:
                new_unit_list.append(unit)
        self.map.unit_list = new_unit_list

    def parse(self):
        return {
            "map": self.map.parse(),
            "players": [player.parse() for player in self.player_list]
        }

    def get_map(self):
        return self.map

    def get_units(self):
        return self.map.unit_list
    
    def get_unit_at(self,pos,flying = None):
        return self.map.get_unit_at(pos,flying)

    def get_unit_by_id(self,id):
        return self.map.get_unit_by_id(id)

    def get_player_by_id(self,id):
        for player in self.player_list:
            if player.camp == id:
                return player
        return None

    def get_artifact_by_id(self,id):
        for player in self.player_list:
            for artifact in player.artifact_list:
                if artifact.id == id:
                    return artifact
        return None

    def get_summon_pos_list(self,player_camp):
        result = [item for item in self.get_miracle_by_id(player_camp).summon_pos_list]
        for barrack in self.get_barracks(player_camp):
            result += barrack.summon_pos_list
        return result

    def get_barracks(self,player_camp):
        return [barrack
            for barrack in self.map.barrack_list
            if barrack.camp == player_camp]

    def get_obstacles(self):
        return self.map.get_obstacles()

    def get_ground_obstacles(self):
        return self.map.get_ground_obstacles()

    def get_flying_obstacles(self):
        return self.map.get_flying_obstacles()

    def get_miracle_by_id(self,player_camp):
        return self.map.get_miracle_by_id(player_camp)
    
    def get_player_score(self,camp=None):
        if camp == None:
            return [player.score for player in self.player_list]
        return self.player_list[camp].score

class SummonListener(EventListener):
    '''
    Only State System register this listener
    '''
    def deal_event(self,event):
        if event.name == "Summon":
            unit = None
            if event.parameter_dict["type"] == "Archer":
                unit = Archer(
                    event.parameter_dict["camp"],
                    event.parameter_dict["level"],
                    event.parameter_dict["pos"],
                    self.host
                )
            elif event.parameter_dict["type"] == "Swordsman":
                unit = Swordsman(
                    event.parameter_dict["camp"],
                    event.parameter_dict["level"],
                    event.parameter_dict["pos"],
                    self.host
                )
            elif event.parameter_dict["type"] == "BlackBat":
                unit = BlackBat(
                    event.parameter_dict["camp"],
                    event.parameter_dict["level"],
                    event.parameter_dict["pos"],
                    self.host
                )
            elif event.parameter_dict["type"] == "Priest":
                unit = Priest(
                    event.parameter_dict["camp"],
                    event.parameter_dict["level"],
                    event.parameter_dict["pos"],
                    self.host
                )
            elif event.parameter_dict["type"] == "VolcanoDragon":
                unit = VolcanoDragon(
                    event.parameter_dict["camp"],
                    event.parameter_dict["level"],
                    event.parameter_dict["pos"],
                    self.host
                )
            elif event.parameter_dict["type"] == "Inferno":
                unit = Inferno(
                    event.parameter_dict["camp"],
                    event.parameter_dict["level"],
                    event.parameter_dict["pos"],
                    self.host,
                    event.parameter_dict["artifact_host"]
                )
            elif event.parameter_dict["type"] == "FrostDragon":
                unit = FrostDragon(
                    event.parameter_dict["camp"],
                    event.parameter_dict["level"],
                    event.parameter_dict["pos"],
                    self.host
                )
            if unit:
                self.host.map.add_unit(unit)
                self.host.emit(Event("Spawn",{
                    "source": unit,
                    "pos": unit.pos
                }))
                self.host.emit(Event("UpdateRingBuff",priority = 3))
                # print("{} (ID: {}) spawns at {}".format(
                #     unit.name,
                #     unit.id,
                #     unit.pos
                # ))

class CheckDeathListener(EventListener):
    '''
    Only State System register this listener
    '''
    def deal_event(self,event):
        if event.name == "CheckDeath":
            for unit in self.host.map.unit_list:
                if unit.hp <= 0 and not unit.death_flag:
                    unit.death_flag = True
                    self.host.emit(Event("Death", {
                        "source": unit
                    }))
                    # print("{} (ID: {}) is announced to be dead.".format(
                    #     unit.name,
                    #     unit.id
                    # ))

class CheckBarrackListener(EventListener):
    '''
    Only State System register this listener
    '''
    def deal_event(self,event):
        if event.name == "CheckBarrack":
            for barrack in self.host.map.barrack_list:
                for unit in self.host.map.unit_list:
                    if unit.pos == barrack.pos and unit.flying == False:
                        barrack.camp = unit.camp
                        break

class TurnStartListener(EventListener):
    '''
    Only State System register this listener
    '''
    def deal_event(self,event):
        if event.name == "TurnStart":
            self.host.emit(Event("Refresh",{
                "camp": self.host.player_list[self.host.current_player_id].camp,
                "turn": self.host.turn_count
            },4))
            self.host.emit(Event("CheckBarrack",{},4))
            self.host.emit(Event("NewTurn",{},4))
    
class ChangeCurrentPlayerListener(EventListener):
    '''
    Only State System register this listener
    '''
    def deal_event(self,event):
        if event.name == "TurnEnd":
            self.host.turn_count += 1
            self.host.current_player_id += 1
            self.host.current_player_id %= len(self.host.player_list)
            # print("Current Player changed to {}".format(
            #     self.host.player_list[self.host.current_player_id].camp
            # ))

class GameStartListener(EventListener):
    '''
    Only State System register this listener
    '''
    def deal_event(self,event):
        if event.name == "GameStart":
            camp = event.parameter_dict["camp"]
            player = self.host.get_player_by_id(camp)
            player.creature_capacity_list = [
                CreatureCapacity(name) \
                for name in event.parameter_dict["cards"]["creatures"]
            ]
            player.artifact_list = [
                gen_artifact_by_name(name,camp,self.host) \
                for name in event.parameter_dict["cards"]["artifacts"]
            ]
            for index, item in enumerate(self.host.player_list):
                if item.camp == player:
                    self.host.player_list[index] = player
                    break
