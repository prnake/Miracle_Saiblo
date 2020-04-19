#/usr/bin/python
# -*- coding:utf-8 -*-
'''
classes of operations
'''

from Geometry import calculator
from StateSystem.Event import Event
from StateSystem.UnitData import UNIT_DATA
from StateSystem.UnitData import ARTIFACTS

def to_position(src):
    '''
    turn a variable to postion (x, y, z)
    '''
    try:
        position = list(src)
        sum_up = 0
        for i in range(3):
            position[i] = int(position[i])
            sum_up += position[i]
        position = tuple(position)
        assert sum_up == 0
        return position
    except Exception:
        raise ValueError("Invalid postion: {}".format(src))

class AbstractOperation:
    '''
    base class of all operations
    '''
    def __init__(self, _parser, _id, _map):
        self.parser = _parser
        self.player_id = _id
        self.map = _map
        self.player = _map.get_player_by_id(_id)
        if self.player is None:
            raise ValueError("Invalid player id: {}".format(_id))

    def check_legality(self):
        '''
        check legality of this operation
        '''

    def act(self):
        '''
        emit action event after legality check
        '''
    def unit_conflict(self, unit, pos):
        '''
        judge if unit will conflict with another unit
        '''
        target = self.map.get_unit_at(pos, flying=unit.flying)
        result = True
        if target is None:
            result = False
        return result

    def get_unit_by_id(self, unit_id):
        '''
        get unit by id
        '''
        try:
            _id = int(unit_id)
            unit = self.map.get_unit_by_id(_id)
            assert unit is not None
            return unit
        except Exception:
            raise ValueError("Invalid unit id: {}".format(unit_id))

    def get_artifact_by_id(self, artifact_id):
        '''
        get artifact by id
        '''
        try:
            _id = int(artifact_id)
            artifact = self.map.get_artifact_by_id(_id)
            assert artifact is not None
            return artifact
        except Exception:
            raise ValueError("Invalid artifact id: {}".format(artifact_id))

class Forbid(AbstractOperation):
    '''
    operation of forbiding artifact and so on
    depreciated
    '''
    def __init__(self, _parser, _id, _map, _params):
        AbstractOperation.__init__(self, _parser, _id, _map)
        self.name = "Forbid"
        self.type = _params["type"]
        self.target = _params["target"]

    def check_legality(self):
        return True

    def act(self):
        pass

class Select(AbstractOperation):
    '''
    operation of selecting artifact and so on
    depreciated
    '''
    def __init__(self, _parser, _id, _map, _params):
        AbstractOperation.__init__(self, _parser, _id, _map)
        self.name = "Select"
        self.type = _params["type"]
        self.target = _params["target"]

    def check_legality(self):
        return True

    def act(self):
        pass

class Init(AbstractOperation):
    '''
    initialize status of a player
    unchecked
    '''
    def __init__(self, _parser, _id, _map, _params):
        AbstractOperation.__init__(self, _parser, _id, _map)
        self.name = "Init"
        self.artifacts = _params["artifacts"]
        self.creatures = _params["creatures"]

    def creature_legality(self):
        all_creatures = list(UNIT_DATA)
        for item in self.creatures:
            if item not in all_creatures:
                return False
        return True

    def artifact_legality(self):
        all_artifacts = []
        for art in ARTIFACTS:
            all_artifacts.append(art)
        for item in self.artifacts:
            if item not in all_artifacts:
                return False
        return True

    def check_legality(self):
        # 是否已经初始化过了?
        if len(self.artifacts) != 1 or len(self.creatures) != 3: # 1张神器，3张生物
            return "Wrong number of cards"
        elif len(self.creatures) != len(set(self.creatures)):    # 生物互不相同
            return "Duplicate creatures"
        elif not self.creature_legality():       # 生物是否存在
            return "Wrong creature name"
        elif not self.artifact_legality():       # 神器是否存在
            return "Wrong artifact name"
        return True

    def act(self):
        self.map.emit(
            Event("GameStart", {
                "camp": int(self.player_id), 
                "cards": {
                        "artifacts": self.artifacts,
                        "creatures": self.creatures
                    }
            }))
        self.map.start_event_processing()

class StartRound(AbstractOperation):
    '''
    start stage of a new round
    unchecked
    '''
    def __init__(self, _parser, _id, _map):
        AbstractOperation.__init__(self, _parser, _id, _map)
    
    def check_legality(self):
        return True

    def act(self):
        self.map.emit(Event("TurnStart"))
        self.map.start_event_processing()

class EndRound(AbstractOperation):
    '''
    end of a round
    unfinished
    '''
    def __init__(self, _parser, _id, _map):
        AbstractOperation.__init__(self, _parser, _id, _map)

    def check_legality(self):
        return True

    def act(self):
        self.map.emit(Event("TurnEnd"))
        self.map.start_event_processing()

class Surrender(AbstractOperation):
    '''
    one player surrender to the other
    '''
    def __init__(self, _parser, _id, _map):
        AbstractOperation.__init__(self, _parser, _id, _map)

    def check_legality(self):
        return True

    def act(self):
        pass

class AbstractAct(AbstractOperation):
    '''
    abstract class for operations in battle(summon, move, attack)
    '''
    def __init__(self, _parser, _id, _map):
        AbstractOperation.__init__(self, _parser, _id, _map)

    def summoned_this_round(self, creature_id):
        '''
        judge if target is summoned this round
        '''
        return creature_id in self.player.newly_summoned_id_list

    def acted_this_round(self, creature_id):
        '''
        check if the creature has acted this round
        '''
        return creature_id in self.parser.moved or creature_id in self.parser.attacked


class Summon(AbstractAct):
    '''
    summon creature
    '''
    def __init__(self, _parser, _id, _map, _params):
        AbstractAct.__init__(self, _parser, _id, _map)
        self.name = "Summon"
        self.type = _params["type"]
        self.level = _params["level"]
        self.position = to_position(_params["position"])
        self.all_type = UNIT_DATA.keys()

    def check_mana_cost(self):
        '''
        check mana cost
        '''
        return self.player.mana >= UNIT_DATA[self.type]["cost"][self.level-1]

    def check_unit_cool_down(self, _type):
        '''
        check if the creature is in cool-down time
        '''
        for creature in self.player.creature_capacity_list:
            if creature.type == _type and creature.available_count > 0:
                return True
        return False
    
    def unit_conflict(self, creature_type, pos):
        '''
        override, check if the summon position already had a creature on it
        '''
        flying = UNIT_DATA[creature_type]["flying"]
        target = self.map.get_unit_at(pos, flying=flying)
        result = True
        if target is None:
            result = False
        return result


    def check_legality(self):
        result = True
        if self.type not in self.all_type:
            result = "Invalid creature type"
        elif self.level not in [1, 2, 3]:
            result = "Invalid level"
        elif self.position not in self.map.get_summon_pos_list(self.player_id):
            result = "No barrack at the point"
        elif self.unit_conflict(self.type, self.position):
            result = "Unit conflict"
        elif not self.check_unit_cool_down(self.type):
            result = "Unit in cooling down"
        elif not self.check_mana_cost():
            result = "Magic cost too high"
        return result

    def act(self):
        self.map.emit(
            Event("Summon", {
                "type": self.type,
                "level": self.level,
                "pos": self.position,
                "camp": self.player_id
            }))
        self.map.start_event_processing()

class Move(AbstractAct):
    '''
    move creature
    '''
    def __init__(self, _parser, _id, _map, _params):
        AbstractAct.__init__(self, _parser, _id, _map)
        self.name = "Move"
        self.mover = self.get_unit_by_id(_params["mover"])
        self.position = to_position(_params["position"])

    def acted_special_check(self):
        if self.mover.agility:
            return True
        return False

    def check_legality(self):
        result = True
        path = calculator.path(self.mover, self.position, self.map)
        if self.mover.camp != self.player_id:
            result = "You cannot manipulate the unit of the other player"
        elif self.unit_conflict(self.mover, self.position):
            result = "Unit conflict: target: {}".format(self.position)
        elif not path:  # no path found
            result = "No suitable path"
        elif self.mover.max_move < len(path)-1: # path include start point, so len need -1
            result = "Out of reach: max move: {}, shortest path: {}"\
                     .format(self.mover.max_move, path)
        elif not self.mover.can_move:
            result = "Has acted this round"
        if result is not True:
            result += "\nstart: {}, end: {}\n".format(self.mover.pos, self.position)
        return result

    def act(self):
        self.parser.moved.append(self.mover.id)
        self.map.emit(
            Event("Move", {
                "source": self.mover,
                "dest": self.position
                }))
        self.map.start_event_processing()

class Attack(AbstractAct):
    '''
    attack operation
    '''
    def __init__(self, _parser, _id, _map, _params):
        AbstractAct.__init__(self, _parser, _id, _map)
        self.name = "Attack"
        self.attacker = self.get_unit_by_id(_params["attacker"])
        target_id = _params["target"]
        if target_id in (0, 1):
            self.target = self.map.get_miracle_by_id(target_id)
        else:
            self.target = self.map.get_unit_by_id(_params["target"])

    def acted_special_check(self):
        if self.attacker.agility:
            return True
        return False

    def check_legality(self):
        result = True
        dist = calculator.cube_distance(self.attacker.pos, self.target.pos)
        if self.attacker.camp != self.player_id:
            result = "You cannot manipulate the unit of the other player"
        elif self.attacker.atk <= 0:
            result = "Attack below zero"
        elif not self.attacker.can_atk:
            result = "Has acted this round"
        elif not self.attacker.atk_range[0] <= dist <= self.attacker.atk_range[-1]:
            result = "Out of range:\nattack range: {}, target distance: {}"\
                    .format(self.attacker.atk_range, dist)
        elif self.target.hp <= 0:
            result = "Target hp <= 0"
        elif self.target.id != 0 and self.target.id != 1 and self.target.flying and not self.attacker.flying and not self.attacker.atk_flying:
            result = "Cannot reach unit in sky"
        if result is not True:
            result += "\nattacker: {}\n, target: {}"\
                .format(self.attacker, self.target)
        return result

    def act(self):
        self.parser.attacked.append(self.attacker.id)
        self.map.emit(
            Event("Attack", {
                "source": self.attacker,
                "target": self.target
                }))
        self.map.start_event_processing()

class Use(AbstractOperation):
    '''
    use artifact
    '''
    def __init__(self, _parser, _id, _map, _params):
        AbstractOperation.__init__(self, _parser, _id, _map)
        self.name = "Use"
        # self.type = _params["type"]
        self.artifact = self.get_artifact_by_id(_params["card"])
        if self.artifact.target_type == "Pos":
            self.target = to_position(_params["target"])
        elif self.artifact.target_type == "Unit":
            self.target = self.get_unit_by_id(_params["target"])
        else:
            self.target = None

    def special_check(self):
        '''
        special check for certain artifact
        '''
        if self.artifact.name == "InfernoFlame":
            miracle = self.map.get_miracle_by_id(self.player_id)
            barracks = self.map.get_barracks(self.player_id)
            in_range = False
            for barrack in barracks:
                if calculator.cube_distance(barrack.pos, self.target) <= 5:
                    in_range = True
            if calculator.cube_distance(miracle.pos, self.target) <= 7:
                in_range = True
            return in_range and self.map.get_unit_at(self.target, flying = False) is None

        elif self.artifact.name == "HolyLight":
            return calculator.in_map(self.target)
        return True

    def check_legality(self):
        result = True
        if self.artifact.camp != self.player_id:
            result = "That's not your artifact"
        elif self.artifact.state != "Ready":
            result = "The artifact is " + self.artifact.state
        elif self.artifact.cost > self.player.mana:
            result = "Insufficient mana"
        elif not self.special_check():
            result = "Conditions not covered"
        return result

    def act(self):
        self.map.emit(
            Event("ActivateArtifact", {
                "camp": self.player_id,
                "name": self.artifact.name,
                "target": self.target
                }))
        self.map.start_event_processing()
