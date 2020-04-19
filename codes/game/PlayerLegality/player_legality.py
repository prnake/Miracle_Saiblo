#!/usr/bin/python
# -*- coding:utf-8 -*-

'''
    player legality check api
'''
from . import operations
from StateSystem.Event import Event
import json

class Parser:
    '''
    class of parser
    '''
    def __init__(self, _map):
        self.map = _map
        self.summoned = []
        self.moved = []
        self.attacked = []
        self.round = 0

    def set_round(self, _round):
        '''
        update round
        '''
        self.summoned = []
        self.moved = []
        self.attacked = []
        self.round = _round
        self.map.emit(Event("TurnStart"))
        self.map.start_event_processing()

    def parse(self, operation_json):
        '''
            parse operation, check legality and emit responding event
        '''
        try:
            operation = json.loads(operation_json)
        except json.decoder.JSONDecodeError:
            raise Exception("error: json decode error")
        try:
            _round = int(operation["round"])
        except Exception:
            raise Exception("error: round error")

        #check round number
        if _round != self.round:
            # return "Not the same round"
            raise Exception("error: not the same round")
        #create operation object
        operation_object = self.to_object(operation)
        if isinstance(operation_object, BaseException):
            #return error message
            #print(operation_object)
            #return operation_object
            raise operation_object
        # check legality
        try:
            legality = operation_object.check_legality()
        except Exception:
            legality = str(Exception)
        if legality is True:
            #emit responding event
            #print("emit " + operation_object.name)
            try:
                operation_object.act()
            except Exception:
                raise Exception("from StateSystem:"+str(Exception))
            #return "OK"
        else:
            #return error message
            #print("emit " + operation_object.name + " error: " + str(legality))
            #return legality
            raise Exception(operation_object.name + " error: " + str(legality))

    def to_object(self, operation_json):
        '''
        convert JSON to corresponding operation object
        '''
        try:
            operation_type = operation_json["operation_type"].lower()
            player_id = int(operation_json["player"])
            params = operation_json["operation_parameters"]
            if operation_type == "forbid":
                operation_object = operations.Forbid(self, player_id, self.map, params)
            elif operation_type == "select":
                operation_object = operations.Select(self, player_id, self.map, params)
            elif operation_type == "summon":
                operation_object = operations.Summon(self, player_id, self.map, params)
            elif operation_type == "move":
                operation_object = operations.Move(self, player_id, self.map, params)
            elif operation_type == "attack":
                operation_object = operations.Attack(self, player_id, self.map, params)
            elif operation_type == "use":
                operation_object = operations.Use(self, player_id, self.map, params)
            elif operation_type == "startround":
                operation_object = operations.StartRound(self, player_id, self.map)
            elif operation_type == "endround":
                operation_object = operations.EndRound(self, player_id, self.map)
            elif operation_type == "surrender":
                operation_object = operations.Surrender(self, player_id, self.map)
            elif operation_type == "init":
                operation_object = operations.Init(self, player_id, self.map, params)
            return operation_object
        except KeyError as error:
            return KeyError("From player legality, KeyError: " + str(error))
        except ValueError as error:
            return ValueError("From player legalit, ValueError: " + str(error).split(':')[-1])
        except Exception as error:
            return Exception("From player legality" + str(error))

if __name__ == "__main__":
    example = {
        "player": "0",
        "operation_type": "Forbid",
        "operation_parameters":{
            }
        }
