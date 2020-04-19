class Obstacle:
    def __init__(self,name,pos):
        self.type = name
        self.pos = pos
        self.allow_flying = name == "Abyss"
        self.allow_ground = False

    def parse(self):
        return {
            "type": self.type,
            "pos": self.pos,
            "allow_flying": self.allow_flying,
            "allow_ground": self.allow_ground
        }

ABYSS_INIT_LIST = [
    (0,0,0),
    (-1,0,1),
    (0,-1,1),
    (1,-1,0),
    (1,0,-1),
    (0,1,-1),
    (-1,1,0),

    (-2,-1,3),
    (-1,-2,3),
    (-2,-2,4),
    (-3,-2,5),

    (-4,-4,8),
    (-5,-4,9),
    (-4,-5,9),
    (-5,-5,10),
    (-6,-5,11),

    (1,2,-3),
    (2,1,-3),
    (2,2,-4),
    (3,2,-5),

    (4,4,-8),
    (5,4,-9),
    (4,5,-9),
    (5,5,-10),
    (6,5,-11)
]