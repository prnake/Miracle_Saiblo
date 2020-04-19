#!/usr/bin/python
# -*- coding:utf-8 -*-
'''
calculator for hex-grids
'''

def MAPBORDER():
    '''
    return map border'''
    # left-up, right-up, left-down, right-down, left-right, up, down
    border = [(-6+i, -9, 15-i) for i in range(0, 14)] + \
                [(9, 6-i, -15+i) for i in range(0, 14)] + \
                [(-9, -6+i, 15-i) for i in range(0, 14)] + \
                [(6-i, 9, -15+i) for i in range(0, 14)] + \
                [(-7, -8, 15), (-8, -7, 15), (8, 7, -15), (7, 8, -15)] + \
                [(7, -8, 1), (8, -8, 0), (8, -7, -1)] + \
                [(-8, 7, 1), (-8, 8, 0), (-7, 8,  -1)]
    return border

def cube_distance(a, b):
    '''
    return distance between two unit, a/b is position
    '''
    try:
        distance = (abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2]))/2
    except KeyError:
        raise ValueError("Point format wrong: a: %s, b:%s"%(str(a), str(b)))
    return int(distance + 1e-8)

def cube_neighbor(pos, dir):
    '''
    neighbor of pos, dir ranges from 0 to 5
    '''
    _dir = dir%6
    if _dir == 0:   # first neighbor
        neighbor = (pos[0]+1, pos[1], pos[2]-1)
    elif _dir == 1:
        neighbor = (pos[0]+1, pos[1]-1, pos[2])
    elif _dir == 2:
        neighbor = (pos[0], pos[1]-1, pos[2]+1)
    elif _dir == 3:
        neighbor = (pos[0]-1, pos[1], pos[2]+1)
    elif _dir == 4:
        neighbor = (pos[0]-1, pos[1]+1, pos[2])
    else:
        neighbor = (pos[0], pos[1]+1, pos[2]-1)
    return neighbor

class Node:
    def __init__(self, pos, G, H, parent=None):
        self.pos = tuple(pos)
        self.G = G
        self.H = H
        self.parent = parent

    def __str__(self):
        return '''
                pos: {},
                G: {},
                F: {},
               '''.format(self.pos, self.G, self.H)

def search_path(start, to, obstacles=[], obstructs=[]):
    '''
    return shortest path
    '''
    #_start = ()
    #_to = ()
    #for i in range(3):
    #    _start += (start[i],)
    #    _to += (to[i],)
    _start = tuple(start)
    _to = tuple(to)
    if _to in obstacles:
        #print("to: " + str(_to))
        #print(obstacles)
        return False
    opened = {}
    closed = {}
    opened[_start] =  Node(start, 0, cube_distance(start, to))
    while opened:
        cur_node = opened[min(opened, key=lambda x: opened[x].G + opened[x].H)]
        #print("Opened: "+str(opened.keys()))
        #print("cur node:" + str(cur_node.pos))
        for i in range(6):
            neighbor = cube_neighbor(cur_node.pos, i)
            if neighbor not in closed and neighbor not in obstacles:
                if neighbor in opened:
                    if cur_node.G+1 < opened[neighbor].G:
                        opened[neighbor].G = cur_node.G + 1
                        opened[neighbor].parent = cur_node
                else:
                    opened[neighbor] = Node(neighbor, cur_node.G+1, cube_distance(neighbor, _to), cur_node) 
                    if neighbor == _to:
                        final_path = []
                        node = opened[neighbor]
                        while node is not None:
                            final_path.insert(0, node.pos)
                            node = node.parent
                        return final_path
                    elif neighbor in obstructs:
                        del opened[neighbor]
        closed[tuple(cur_node.pos)] = cur_node
        del opened[tuple(cur_node.pos)]
    return False

def cube_reachable(start, movement, obstacles=[], obstructs=[]):
    '''
    return reachable position from start point in steps limited by movement
    '''
    visited = []    # positions that have been visited
    visited.append(start)
    fringes = []    # list of list of reachable points in certain steps(subscripts means steps)
    fringes.append([start])

    for i in range(0, movement):
        fringes.append([])
        for pos in fringes[i]:
            if pos in obstructs:
                continue
            for j in range(0, 6):
                neighbor = cube_neighbor(pos, j)
                if neighbor not in visited and neighbor not in obstacles\
                        and in_map(neighbor):
                    visited.append(neighbor)
                    fringes[i+1].append(neighbor)
    return fringes

def get_obstacles_by_unit(unit, _map):
    '''
    returns all obstacles for a unit
    '''
    obstacles = MAPBORDER()
    #obstacles=[]
    if unit.flying:
        fixed_obstacles = _map.get_flying_obstacles()
    else:
        fixed_obstacles = _map.get_ground_obstacles()
    for obstacle in fixed_obstacles:
        obstacles.append(obstacle.pos)
    obstacle_unit = _map.get_units()
    for obstacle in obstacle_unit:
        #if obstacle.camp != unit.camp:
        if unit.flying == obstacle.flying:
            obstacles.append(obstacle.pos)
    return obstacles

def get_obstructs_by_unit(unit, _map):
    '''
    returns all obstructs for a unit, obstructs means the unit can
    stay at that point but cannot pass it
    '''
    obstructs = []
    obstacle_unit = _map.get_units()
    for obstruct in obstacle_unit:
        if obstruct.camp != unit.camp:
            if obstruct.flying == unit.flying:
                for i in range(0, 6):
                    obstructs.append(cube_neighbor(obstruct.pos, i))
            else:
                obstructs.append(obstruct.pos)
    if unit.pos in obstructs:
        obstructs.remove(unit.pos)
    return obstructs

'''
below are public sdk
'''

def path(unit, dest, _map):
    '''
    public sdk for search_path
    '''
    obstacles = get_obstacles_by_unit(unit, _map)
    #print("mapborder: "+str(MAPBORDER()))
    #print("obstacles:" + str(obstacles))
    obstructs = get_obstructs_by_unit(unit, _map)
    #print("obstructs:" + str(obstructs))
    result = search_path(unit.pos, dest, obstacles, obstructs)
    #print("Path:" + str(result))
    return result

def reachable(unit, _map):
    '''
    public sdk for cube_reachable
    '''
    obstacles = get_obstacles_by_unit(unit, _map)
    obstructs = get_obstructs_by_unit(unit, _map)
    result = cube_reachable(unit.pos, unit.max_move, obstacles, obstructs)
    return result

def units_in_range(pos, dist, _map, camp=-1, flyingIncluded=True, onlandIncluded=True):
    '''
    return list of units whose distance to the pos is less than dist
    default camp = -1, return units of both camp, 0 for the first camp, 1 for the second
    flyingIncluded = True will include flying units,
    onlandIncluded = True will include onland units
    '''
    units = []
    all_units = _map.get_units()
    for _unit in all_units:
        if cube_distance(_unit.pos, pos) <= dist and \
           (camp == -1 or camp == _unit.camp) and \
           ((_unit.flying and flyingIncluded) or \
           (not _unit.flying and onlandIncluded)):
               units.append(_unit)
    return units

def in_map(pos):
    '''
    return if the position in inside the map
    '''
    if pos[0] > 8 or pos[0] < -8 or \
       pos[1] > 8 or pos[1] < -8 or \
       pos[2] >14 or pos[2] < -14:
        return False
    elif pos == (-7, 7, 0) or pos == (7, -7, 0):
        return False
    return True

def all_pos_in_map():
    '''
    return all positions in map
    '''
    all_pos = []
    for i in range(-8, 9):
        for j in range(-8, 9):
            cur_pos = (i, j, -(i+j))
            if in_map(cur_pos):
                all_pos.append(cur_pos)
    return all_pos

if __name__ == "__main__":
    print(MAPBORDER())
