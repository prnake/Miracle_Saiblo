'''游戏主体逻辑
'''

import sys
import json
import random
from PlayerLegality.player_legality import Parser
from StateSystem.StateSystem import StateSystem

DEBUG = False  # DEBUG时会生成一个log.txt记录logic收发的信息
MAX_ROUND = 100
AI_TIME = 3
PLAYER_TIME = 300


def logic_convert_byte(data_str, send_goal):
    '''传输数据的时候加数据长度作为数据头
    '''
    message_len = len(data_str)
    message = message_len.to_bytes(4, byteorder='big', signed=True)
    message += send_goal.to_bytes(4, byteorder='big', signed=True)
    if isinstance(data_str, str):
        message += bytes(data_str, encoding="utf8")
    elif isinstance(data_str, bytes):
        message += data_str
    return message


def read_opt():
    '''读取发过来的操作
    '''
    read_buffer = sys.stdin.buffer
    data_len = int.from_bytes(read_buffer.read(
        4), byteorder='big', signed=True)
    data = read_buffer.read(data_len)
    opt = json.loads(data)
    return opt


def send_end_info(end_info):
    '''发送终局信息
    '''
    end_dict = {}
    end_dict['state'] = -1
    end_dict['end_info'] = json.dumps(end_info)
    sys.stdout.buffer.write(logic_convert_byte(json.dumps(end_dict), -1))
    sys.stdout.flush()


def send_init(time, length):
    '''发送初始化信息
    '''
    sys.stdout.buffer.write(logic_convert_byte(
        json.dumps({"state": 0, "time": time, "length": length}), -1))
    sys.stdout.flush()


def send_state(state_dict):
    '''发送回合信息

    Args:
        state_dict: dict
    '''
    sys.stdout.buffer.write(logic_convert_byte(json.dumps(state_dict), -1))
    sys.stdout.flush()


class Game:
    '''游戏 神迹之战Miracle
    '''
    # pylint: disable=too-many-instance-attributes

    def __init__(self):
        '''初始化变量
        '''
        self.media_players = []  # 播放器
        self.audience = []      # 观众
        self.replay = ""        # 录像文件存储处
        self.state = 0          # 当前消息回合
        self.listen = 0         # 当前监听的玩家(当前回合玩家)
        self._round = -1        # 当前游戏回合
        self.is_end = False     # 是否结束
        self.statesystem = StateSystem()
        self.parser = Parser(self.statesystem)
        self.map_type = random.randint(0, 1)  # 地图类型
        self.day_time = random.randint(0, 1)  # 地图时间

    def check_game_end(self):
        '''判断游戏是否结束，若结束则结束对局
        '''
        miracle_hp = [self.statesystem.get_miracle_by_id(0).hp,
                      self.statesystem.get_miracle_by_id(1).hp]
        if self._round >= MAX_ROUND or miracle_hp[0] <= 0 or miracle_hp[1] <= 0:
            self.end(self.statesystem.get_player_score(0), self.statesystem.get_player_score(1))

    def change_round(self):
        '''切换到下一个玩家，其回合开始
        '''
        self.state += 1
        self.listen = 1 - self.listen
        if self.listen in self.media_players:
            send_init(PLAYER_TIME, 1024)
        else:
            send_init(AI_TIME, 1024)

        self._round += 1
        self.parser.set_round(self._round)

    def get_round_ope(self):
        '''一个游戏回合(主要阶段)内的操作
        '''
        while not self.is_end:
            self.send_game_info()
            opt_dict = read_opt()
            while opt_dict["player"] == 1 - self.listen:
                opt_dict = read_opt()
            if opt_dict["player"] == self.listen:
                try:
                    self.parser.parse(opt_dict["content"])
                except Exception as parse_error:
                    self.send_media_info(
                        [self._round, -1, 0, 0, 0, 0, 0], self.listen)
                    if DEBUG:
                        with open('log.txt', 'a') as logfile:
                            logfile.write(opt_dict["content"]+',\n\n')
                            logfile.write(parse_error.__repr__()+'\n\n')
                    continue
                else:
                    if DEBUG:
                        with open('log.txt', 'a') as logfile:
                            logfile.write(opt_dict["content"]+',\n\n')
                self.send_media_info()
                self.check_game_end()
                # 结束回合 / 投降
                special_type = json.loads(opt_dict["content"])["operation_type"]
                if special_type == "endround":
                    break
                if special_type == "surrender":
                    if opt_dict['player'] == 0:
                        self.end(0, self.statesystem.get_player_score(1) + 1)
                    else:
                        self.end(self.statesystem.get_player_score(0) + 1, 0)
            # AI异常
            elif opt_dict['player'] == -1:
                opt = json.loads(opt_dict['content'])
                # AI异常退出
                if opt['error'] == 0:
                    if opt['player'] == 0:
                        self.end(0, self.statesystem.get_player_score(1) + 1)
                    else:
                        self.end(self.statesystem.get_player_score(0) + 1, 0)
                # 超时
                # else:
                elif opt['state'] == self.state and opt['player'] == self.listen:
                    if DEBUG:
                        with open('log.txt', 'a') as logfile:
                            logfile.write('timeout!\n\n')
                    if self.listen == -1:
                        msg = json.dumps(
                            {"player": 0 if self.listen == 0 else 1,
                             "round": self._round,
                             "operation_type": "endround",
                             "operation_parameters": {}})
                        self.parser.parse(msg)
                    else:
                        if opt['player'] == 0:
                            self.end(0, self.statesystem.get_player_score(1) + 1)
                        else:
                            self.end(self.statesystem.get_player_score(0) + 1, 0)
                    break

    def get_media_info(self, events):
        '''把事件数组转为给播放器的整数数组

        Args:
            events: 事件数组

        Returns:
            int list
        '''
        # pylint: disable=too-many-branches
        # pylint: disable=too-many-statements
        event_names = ["", "TurnStart", "TurnEnd", "Spawn", "Move", "Attack",
                       "Damage", "Death", "Heal", "ActivateArtifact",
                       "GameEnd", "GameStart", "BuffAdd", "BuffRemove",
                       "Attacking", "Attacked", "Leave", "Arrive", "Summon"]
        creature_names = ["", "Swordsman", "Archer", "BlackBat", "Priest",
                          "VolcanoDragon", "FrostDragon", "Inferno"]
        artifact_names = ["", "HolyLight", "SalamanderShield", "InfernoFlame",
                          "WindBlessing"]

        media_info = []
        for event in events:
            if not event.name in event_names:
                continue
            # round
            media_info.append(self._round)
            # event
            media_info.append(event_names.index(event.name))
            if event.name == "TurnStart" or event.name == "TurnEnd":
                # camp
                media_info.append(self._round % 2)
            elif event.name == "Spawn":
                # type
                media_info.append(creature_names.index(
                    event.parameter_dict['source'].type) + 10 * event.parameter_dict['source'].camp)
                # level
                media_info.append(event.parameter_dict['source'].level)
                # posX
                media_info.append(event.parameter_dict['pos'][0])
                # posY
                media_info.append(event.parameter_dict['pos'][1])
                # id
                media_info.append(event.parameter_dict['source'].id)
            elif event.name == "Move":
                # id
                media_info.append(event.parameter_dict['source'].id)
                # desX
                media_info.append(event.parameter_dict['dest'][0])
                # desY
                media_info.append(event.parameter_dict['dest'][1])
            elif event.name == "Leave" or event.name == "Arrive":
                # id
                media_info.append(event.parameter_dict['source'].id)
                # posX
                media_info.append(event.parameter_dict['pos'][0])
                # posY
                media_info.append(event.parameter_dict['pos'][1])
            elif event.name == "Attack" or event.name == "Attacking" or event.name == "Attacked":
                # id1
                media_info.append(event.parameter_dict['source'].id)
                # id2
                media_info.append(event.parameter_dict['target'].id)
            elif event.name == "Damage":
                # id2
                media_info.append(event.parameter_dict['target'].id)
                # id1
                media_info.append(event.parameter_dict['source'].id)
                # damage
                media_info.append(event.parameter_dict['damage'])
                # type
                damage_type = ["", "Attack", "AttackBack",
                               "VolcanoDragonSplash", "InfernoFlameActivate"]
                media_info.append(damage_type.index(
                    event.parameter_dict['type']))
            elif event.name == "Death":
                # id
                media_info.append(event.parameter_dict['source'].id)
            elif event.name == "Heal":
                # id2
                media_info.append(event.parameter_dict['target'].id)
                # id1
                media_info.append(event.parameter_dict['source'].id)
                # h
                media_info.append(event.parameter_dict['heal'])
            elif event.name == "ActivateArtifact":
                # camp
                media_info.append(event.parameter_dict['camp'])
                media_info.append(artifact_names.index(
                    event.parameter_dict['name']) + 10 * event.parameter_dict['camp'])
                if event.parameter_dict['name'] in ["HolyLight", "InfernoFlame", "WindBlessing"]:
                    media_info.append(event.parameter_dict['target'][0])
                    media_info.append(event.parameter_dict['target'][1])
                elif event.parameter_dict['name'] in ["SalamanderShield"]:
                    media_info.append(0)
                    media_info.append(0)
                    media_info.append(event.parameter_dict['target'].id)
            elif event.name == "GameStart":
                # camp
                media_info.append(event.parameter_dict['camp'])
                # a0
                media_info.append(artifact_names.index(
                    event.parameter_dict['cards']["artifacts"][0]) +
                    10 * event.parameter_dict['camp'])
                # c1 c2 c3
                for creature_name in event.parameter_dict['cards']["creatures"]:
                    media_info.append(creature_names.index(
                        creature_name) + 10 * event.parameter_dict['camp'])
            elif event.name == "BuffAdd" or event.name == "BuffRemove":
                # id0
                media_info.append(event.parameter_dict['source'].id)
                # type
                buff_names = ["BaseBuff", "PriestAtkBuff", "HolyShield",
                              "HolyLightAtkBuff", "SalamanderShieldBuff"]
                media_info.append(buff_names.index(
                    event.parameter_dict['type']))
            elif event.name == "Summon":
                # type
                media_info.append(creature_names.index(
                    event.parameter_dict['type']) + 10 * event.parameter_dict['camp'])
                # level
                media_info.append(event.parameter_dict['level'])
                # posX
                media_info.append(event.parameter_dict['pos'][0])
                # posY
                media_info.append(event.parameter_dict['pos'][1])
            if len(media_info) % 7 != 0:
                media_info += [0] * (7-len(media_info) % 7)
        return media_info

    def send_media_info(self, media_info_list=None, goal=3, new=False):
        '''把信息发给播放器或记录于录像文件

        Args:
            media_info: 给播放器的信息整数数组 为空时会直接从事件堆中取

            goal: 发送的目标 -1表示录像文件 0表示播放器玩家0 1表示播放器玩家1 默认全部发送
        '''
        if new:
            with open(self.replay, 'wb') as replay_file:
                for media_info in [0, 0, 0, self.map_type, self.day_time, 0, 0]:
                    replay_file.write(
                        int(media_info).to_bytes(4, 'big', signed=True))
            return
        if not media_info_list:
            if not self.statesystem.event_heap.record:
                return
            media_info_list = self.get_media_info(
                self.statesystem.event_heap.record)
            self.statesystem.event_heap.record.clear()
            if not media_info_list:
                return
        if goal == 3:
            self.send_media_info(media_info_list, -1)
            self.send_media_info(media_info_list, 0)
            self.send_media_info(media_info_list, 1)
        if goal == -1:
            with open(self.replay, 'ab') as replay_file:
                for media_info in media_info_list:
                    replay_file.write(
                        int(media_info).to_bytes(4, 'big', signed=True))
        elif goal in (0, 1):
            if goal in self.media_players:
                send_state({'state': self.state, 'listen': [self.listen],
                            'player': [goal],
                            'content': [json.dumps(media_info_list)]})

    def send_game_info(self):
        '''向当前回合的AI发送游戏当前局面信息
        '''
        if self.listen in self.media_players:
            return
        state_dict = {'state': self.state, 'listen': [self.listen],
                      'player': [self.listen], 'content': []}
        message = dict()
        if self._round == -1:
            message = {'camp': 0 if self.listen == 0 else 1}
        else:
            message = self.statesystem.parse()
            message['round'] = self._round
            message['camp'] = 0 if self.listen == 0 else 1
        # 前六位表示长度 后面是表示信息的json格式字符串
        message_json = json.dumps(message).replace(" ", "")
        json_length = str(len(message_json))
        state_dict['content'] = [
            "0" * (6 - len(json_length)) + json_length + message_json]
        send_state(state_dict)

    def init(self):
        '''根据玩家列表、录像文件地址进行初始化处理

        player_list:
            [1, 0, 2]表示0号玩家为本地AI或者远程算力连接，1号玩家未正常启动进程，2号玩家是远程连接播放器
            0: 该玩家进入游戏失败
            1: 该玩家正常进入游戏，且为评测机本地AI或者远程算力
            2: 该玩家正常进入游戏，且为远程连接播放器
        '''
        opt_dict = read_opt()
        self.replay = opt_dict['replay']
        self.send_media_info(new=True)
        player_list = opt_dict['player_list']
        if player_list[0] == 0:
            self.end(0, 1)
        if player_list[1] == 0:
            self.end(1, 0)
        for player, status in enumerate(player_list):
            if status == 2:
                self.media_players.append(player)

    def select_cards(self):
        '''玩家选择初始卡组
        '''
        media_players_info = [
            [0, 0, 0, self.map_type, self.day_time, 0, 0],
            [0, 0, 1, self.map_type, self.day_time, 0, 0]]
        is_players_ready = [False, False]
        for player in [0, 1]:
            self.state += 1
            self.listen = player
            if player in self.media_players:
                send_init(PLAYER_TIME, 1024)
                self.send_media_info(media_players_info[player], player)
            else:
                send_init(10, 1024)
                self.send_game_info()
            opt_dict = read_opt()
            while opt_dict["player"] == 1 - player:
                opt_dict = read_opt()
            if opt_dict["player"] == -1:
                error_player = json.loads(opt_dict['content'])["player"]
                if error_player == 0:
                    self.end(0, 1)
                else:
                    self.end(1, 0)
            if opt_dict["player"] == player:
                try:
                    self.parser.parse(opt_dict["content"])
                except Exception as parse_error:
                    if DEBUG:
                        with open('log.txt', 'a') as logfile:
                            logfile.write(opt_dict["content"]+',\n\n')
                            logfile.write(parse_error.__repr__()+'\n\n')
                else:
                    if DEBUG:
                        with open('log.txt', 'a') as logfile:
                            logfile.write(opt_dict["content"]+',\n\n')
                    is_players_ready[player] = True
            else:
                is_players_ready[json.loads(opt_dict['content'])["player"]] = False

            if player == 0:
                if not is_players_ready[0]:
                    self.end(0, 1)
            elif not is_players_ready[1]:
                self.end(1, 0)

    def start(self):
        '''开始游戏
        '''
        self.init()
        self.select_cards()
        while not self.is_end:
            self.change_round()
            self.send_media_info()
            self.check_game_end()
            self.get_round_ope()
            self.send_media_info()
            self.check_game_end()

    def end(self, player0_score, player1_score):
        '''游戏终局处理

        Args:
            player0_score: 先手玩家得分
            player1_score: 后手玩家得分
        '''
        self.is_end = True
        if player0_score == player1_score:
            player1_score += 1
        winner = 0 if player0_score > player1_score else 1
        media_info_list = [self._round, 10, winner, 0, 0, 0, 0]
        self.send_media_info(media_info_list)
        self.send_media_info([-1], -1)
        if DEBUG:
            with open('log.txt', 'a') as logfile:
                logfile.write('player '+str(winner)+' win!'+'\n\n')
        end_info = {"0": player0_score, "1": player1_score}
        send_end_info(end_info)
        sys.exit()


if __name__ == '__main__':
    game = Game()
    game.start()
