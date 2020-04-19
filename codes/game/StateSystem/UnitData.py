import json
import os

currentPath = os.path.dirname(__file__)

DATA = json.load(open(currentPath + "/Data.json","r"))

CREATURE_CAPACITY_LEVEL_UP_TURN = DATA["CreatureCapacityLevelUpTurn"]

UNIT_DATA = DATA["UnitData"]

ARTIFACTS = DATA["Artifacts"]

UNIT_NAME_PARSED = DATA["UnitNameParsed"]

ARTIFACT_NAME_PARSED = DATA["ArtifactNameParsed"]

ARTIFACT_STATE_PARSED = DATA["ArtifactStateParsed"]

ARTIFACT_TARGET_PARSED = DATA["ArtifactTargetParsed"]