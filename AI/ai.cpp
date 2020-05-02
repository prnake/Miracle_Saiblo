#include "ai_client.h"
#include "gameunit.h"
#include "card.hpp"
#include "calculator.h"
#include "points.h"

#include <fstream>
#include <stdlib.h>
#include <random>
#include <time.h>
#include <list>

using gameunit::Pos;
using gameunit::Unit;

using calculator::all_pos_in_map;
using calculator::cube_distance;
using calculator::cube_reachable;
using calculator::reachable;
using calculator::units_in_range;

using card::CARD_DICT;

using std::default_random_engine;
using std::get;
using std::make_tuple;
using std::map;
using std::string;
using std::uniform_int_distribution;
using std::vector;
using std::list;

struct unit_info
{
    /*
    int id;                     // id
    int camp;                   // 阵营
    std::string type;           // 种类
    int cost;                   // 法力消耗
    int atk;                    // 攻击
    int max_hp;                 // 生命上限
    int hp;                     // 当前生命
    std::vector<int> atk_range; // 最小攻击范围 最大攻击范围
    int max_move;               // 行动力
    int cool_down;              // 冷却时间
    Pos pos;                    // 位置
    int level;                  // 等级
    bool flying;                // 是否飞行
    bool atk_flying;            // 是否对空
    bool agility;               // 是否迅捷
    bool holy_shield;           // 有无圣盾
    bool can_atk;               // 能否攻击
    bool can_move;              // 能否移动
    */
    unit_info()
    {
        creat_time = -1;
        priority = 0;
        des = 0;
    }
    int creat_time;             //创建时间
    int priority;
    std::list<Pos> history;     //历史路径
    Pos now;
    string target;              //预测行为
    string type;                //分配对策
    int des;                    //我方单位目标 des=1,占领destination;des=2,直线进攻基地
    Pos destination;

};

struct trends
{
    int enemy_miracle;
    int my_miracle;
    int my_barrack, enemy_barrack, right_barrack, left_barrack;
    vector<string> stratergy;
};

class AI : public AiClient
{
private:
    std::vector<unit_info> unit_extra_info;

    struct trends game_trend;

    Pos miracle_pos;

    Pos enemy_miracle_pos;

    Pos target_barrack;

    Pos my_barrack, enemy_barrack, right_barrack, left_barrack;

    Pos posShift(Pos pos, string direct, const int &lenth);

    Pos counter(vector<Pos> &pos_list, string type, int number, int range);

public:
    //选择初始卡组
    void chooseCards(); //(根据初始阵营)选择初始卡组

    void use_inferno();

    void scan_enemy();

    void march_before_battle(const string &type);

    void march_after_battle(const string &type);

    void creat_unit(const string &type);

    void battle(const string &type); //处理生物的战斗

    void play(); //玩家需要编写的ai操作函数
};

void AI::chooseCards()
{
    // (根据初始阵营)选择初始卡组

    /*
     * artifacts和creatures可以修改
     * 【进阶】在选择卡牌时，就已经知道了自己的所在阵营和先后手，因此可以在此处根据先后手的不同设置不同的卡组和神器
     */

    //先手，选择地狱火，弓箭手，牧师，冰冻龙
    if (my_camp == 0)
    {
        my_artifacts = {"InfernoFlame"};
        my_creatures = {"Archer", "Priest", "FrostDragon"};
        init();
    }
    //后手，选择地狱火，弓箭手，牧师，冰冻龙（后期改动）
    else
    {
        my_artifacts = {"InfernoFlame"};
        my_creatures = {"Archer", "Priest", "FrostDragon"};
        init();
    }
        
}

void AI::play()
{
    //玩家需要编写的ai操作函数

    /*
    本AI采用这样的策略：
    在首回合进行初期设置、在神迹优势路侧前方的出兵点召唤一个1星弓箭手
    接下来的每回合，首先尽可能使用神器，接着执行生物的战斗，然后对于没有进行战斗的生物，执行移动，最后进行召唤
    在费用较低时尽可能召唤星级为1的兵，优先度剑士>弓箭手>火山龙
    【进阶】可以对局面进行评估，优化神器的使用时机、调整每个生物行动的顺序、调整召唤的位置和生物种类、星级等
    */
    if (round == 0 || round == 1)
    {
        //先确定自己的基地、对方的基地
        if (my_camp==0)
        {
            miracle_pos = map.miracles[0].pos;
            enemy_miracle_pos = map.miracles[1].pos;
            my_barrack = map.barracks[3].pos;
            enemy_barrack = map.barracks[2].pos;
            left_barrack = map.barracks[0].pos;
            right_barrack = map.barracks[1].pos;
        }
        else
        {
            miracle_pos = map.miracles[1].pos;
            enemy_miracle_pos = map.miracles[0].pos;
            my_barrack = map.barracks[2].pos;
            enemy_barrack = map.barracks[3].pos;
            left_barrack = map.barracks[1].pos;
            right_barrack = map.barracks[0].pos;
        }

        //设定目标驻扎点为最近的驻扎点
        target_barrack = my_barrack;

        /*
        for (const auto &barrack : map.barracks)
        {
            if (cube_distance(miracle_pos, barrack.pos) <
                cube_distance(miracle_pos, target_barrack))
                target_barrack = barrack.pos;
        }
        */

        // 在正中心偏右召唤一个牧师，用来抢占驻扎点
        summon("Priest", 1, posShift(miracle_pos, "SF",1));
    }
    else
    {
        //神器能用就用，选择覆盖单位数最多的地点
        use_inferno();

        scan_enemy();

        march_before_battle("attack");

        battle("attack");

        march_after_battle("attack");

        creat_unit("attack");

    }
        endRound();
}

void AI::scan_enemy()
{
    
    auto enemy_list = getUnitsByCamp(my_camp^1);

    for (const auto &enemy : enemy_list)
    {
        if(enemy.id>=unit_extra_info.size())
            unit_extra_info.resize(enemy.id * 2);
        struct unit_info &extra = unit_extra_info.at(enemy.id);
        if(extra.creat_time == -1)
        {
            extra.creat_time = round;
            extra.now = enemy.pos;
            if (enemy.type == "Archer")
            {
                extra.type == "attack";
                extra.priority = enemy.level + enemy.atk;
            }
            else if (enemy.type == "Swordsman")
            {
                extra.type == "attack";
                extra.priority = enemy.level + enemy.atk;
            }
            else if (enemy.type == "BlackBat")
            {
                extra.type == "attack";
                extra.priority = enemy.level + enemy.atk;
            }
            else if (enemy.type == "Priest")
            {
                extra.type == "attack";
                extra.priority = enemy.level;
                if(enemy.level==1)
                {
                    auto atk_up_enemy = units_in_range(extra.now, 2, map, my_camp ^ 1);
                    extra.priority += atk_up_enemy.size() * 2;
                }
                else
                {
                    auto atk_up_enemy = units_in_range(extra.now, 3, map, my_camp ^ 1);
                    extra.priority += atk_up_enemy.size() * 2;
                }
            }
            else if (enemy.type == "VolcanoDragon")
            {
                extra.type == "attack";
                extra.priority = enemy.level + enemy.atk;
            }
            else if (enemy.type == "Inferno")
            {
                if(cube_distance(extra.now,miracle_pos)<=3)
                {
                    extra.type == "attack";
                    extra.priority = 100;
                }
                else
                {
                    extra.type == "avoid";
                    extra.priority = 2;
                }   
            }
            else if (enemy.type == "FrostDragon")
            {
                extra.type == "attack";
                extra.priority = enemy.level + enemy.atk;
            }
        }
        else
        {
            extra.history.push_front(extra.now);
            extra.now = enemy.pos;

            //粗劣的轨迹分析
            int history_distance[] = {
                cube_distance(extra.history[0], enemy_miracle_pos),
                cube_distance(extra.history[0], enemy_barrack),
                cube_distance(extra.history[0], my_barrack),
                cube_distance(extra.history[0], miracle_pos)
            }

            int now_distance[] = {
                cube_distance(extra.now, enemy_miracle_pos),
                cube_distance(extra.now, enemy_barrack),
                cube_distance(extra.now, my_barrack),
                cube_distance(extra.now, miracle_pos)
            }

            int closest = -1,
                min = 9999;
            for (int i = 0; i < 4; i++)
            {
                if (min > now_distance && now_distance[i] <= history_distance[i])
                {
                    min = now_distance[i];
                    closest = i;
                }
            }
            if (closest == 0)
            {
                extra.target = "enemy_miracle";
            }
            else if (closest == 1)
            {
                extra.target = "enemy_barrack";
            }
            else if (closest == 2)
            {
                extra.target = "my_barrack";
            }
            else if (closest == 3)
            {
                extra.target = "my_miracle";
            }
            else
            {
                extra.target = "chased";
            }

            if (enemy.type == "Archer")
            {
                if (now_distance[3]<=7)
                {
                    extra.target = "my_miracle";
                }
                else if (checkBarrack(enemy_barrack) != my_camp ^ 1 && history_distance[1] > now_distance[1])
                {
                    extra.target = "enemy_barrack";
                }
                else if (checkBarrack(my_barrack) != my_camp ^ 1 && history_distance[2] > now_distance[2])
                {
                    extra.target = "my_barrack";
                }
                extra.type == "attack";
                extra.priority = enemy.level + enemy.atk;
                if (now_distance[3] >= 3 &&now_distance [3] <= 4)
                    extra.priority += enemy.atk*10;
            }
            else if (enemy.type == "Swordsman")
            {
                if (now_distance[3] <= 4)
                {
                    extra.target = "my_miracle";
                }
                else if (checkBarrack(enemy_barrack) != my_camp ^ 1 && history_distance[1] > now_distance[1])
                {
                    extra.target = "enemy_barrack";
                }
                else if (checkBarrack(my_barrack) != my_camp ^ 1 && history_distance[2] > now_distance[2])
                {
                    extra.target = "my_barrack";
                }
                extra.type == "attack";
                extra.priority = enemy.level + enemy.atk;
                if (now_distance[3] ==1)
                    extra.priority += enemy.atk * 10;
            }
            else if (enemy.type == "BlackBat")
            {
                if (history_distance[3] > now_distance[3])
                {
                    extra.target = "my_miracle";
                }
                else if (now_distance[0]<=7 && history_distance[0] > now_distance[0])
                {
                    extra.target = "enemy_miracle";
                }
                else
                {
                    extra.target = "chased";
                }

                extra.type == "avoid";
                extra.priority = enemy.level + enemy.atk;

                if (now_distance[3] <= 5)
                {
                    extra.type == "attack";
                    game_trend.stratergy.push_back("mircle_prevent_bat");
                    extra.priority += enemy.atk * 10;
                }
                    
            }
            else if (enemy.type == "Priest")
            {
                if (checkBarrack(my_barrack) != my_camp ^ 1 && history_distance[2] > now_distance[2])
                {
                    extra.target = "my_barrack";
                }
                else if (now_distance[3] <= 4)
                {
                    extra.target = "my_miracle";
                }
                else if (checkBarrack(enemy_barrack) != my_camp ^ 1 && history_distance[1] > now_distance[1])
                {
                    extra.target = "enemy_barrack";
                }
                extra.type == "attack";
                extra.priority = enemy.level;
                if (enemy.level == 1)
                {
                    auto atk_up_enemy = units_in_range(extra.now, 2, map, my_camp ^ 1);
                    extra.priority += atk_up_enemy.size() * 2;
                }
                else
                {
                    auto atk_up_enemy = units_in_range(extra.now, 3, map, my_camp ^ 1);
                    extra.priority += atk_up_enemy.size() * 2;
                }
                if (now_distance[3] <= 1)
                {
                    extra.type == "attack";
                    extra.priority *= 2;
                }
            }
            else if (enemy.type == "VolcanoDragon")
            {
                if (now_distance[3] <= 3)
                {
                    extra.target = "my_miracle";
                }
                else if (checkBarrack(enemy_barrack) != my_camp ^ 1 && history_distance[1] > now_distance[1])
                {
                    extra.target = "enemy_barrack";
                }
                else if (checkBarrack(my_barrack) != my_camp ^ 1 && history_distance[2] > now_distance[2])
                {
                    extra.target = "my_barrack";
                }
                extra.type == "attack";
                extra.priority = enemy.level + enemy.atk;
                if (now_distance[3] <= 2)
                    extra.priority += enemy.atk * 10;
            }
            else if (enemy.type == "Inferno")
            {
                if (cube_distance(extra.now, miracle_pos) <= 3)
                {
                    extra.type == "attack";
                    extra.priority = 100;
                }
                else
                {
                    extra.type == "avoid";
                    extra.priority = 2;
                }
            }
            else if (enemy.type == "FrostDragon")
            {
                if (now_distance[3] <= 3)
                {
                    extra.target = "my_miracle";
                }
                else if (checkBarrack(enemy_barrack) != my_camp ^ 1 && history_distance[1] > now_distance[1])
                {
                    extra.target = "enemy_barrack";
                }
                else if (checkBarrack(my_barrack) != my_camp ^ 1 && history_distance[2] > now_distance[2])
                {
                    extra.target = "my_barrack";
                }
                extra.type == "attack";
                extra.priority = enemy.level + enemy.atk;
                if (now_distance[3] <= 2)
                    extra.priority += enemy.atk * 10;
            }
        }
        
    }
}

void AI::march_before_battle(const string &type) //处理不得不移动的部分——占点/冲塔；简单策略：能占到目标点/被不能被攻击到的人打掉的血量最小且小于HP
{

    /*
    if(type == "attack")
    {
        auto ally_list = getUnitsByCamp(my_camp);
        auto enemy_list = getUnitsByCamp(my_camp ^ 1);
        //自定义排列顺序
        auto cmp = [](const Unit &unit1, const Unit &unit2) {
            if (unit1.can_move != unit2.can_move) //首先要能动
                return unit2.can_move < unit1.can_move;
            else if (unit1.type != unit2.type)
            { //冰龙>弓箭手>牧师
                auto type_id_gen = [](const string &type_name) {
                    if (type_name == "FrostDragon")
                        return 0;
                    else if (type_name == "Archer")
                        return 1;
                    else
                        return 2;
                };
                return (type_id_gen(unit1.type) < type_id_gen(unit2.type));
            }
            else
                return unit2.atk < unit1.atk;
        };

        sort(ally_list.begin(), ally_list.end(), cmp);

        for (const auto &ally : ally_list)
        {
            if (ally.can_atk)
            {
                int dis = cube_distance(ally.pos, enemy_miracle_pos);
                if (ally.atk_range[0] <= dis && dis <= ally.atk_range[1]){
                        unit_extra_info[ally.id].type = "attack";
                        unit_extra_info[ally.id].attack_id = my_camp ^ 1;
                }

                vector<Unit> target_list;
                for (const auto &enemy : enemy_list)
                    if (AiClient::canAttack(ally, enemy))
                        target_list.push_back(enemy);
                if (!target_list.empty())
                {
                    if (ally.type == "FrostDragon")
                    {
                        int max_score = -1;
                        int tar = -1;
                        for (auto target : target_list)
                        {
                            int score = 0;
                            struct unit_info &extra = unit_extra_info[target.id];
                            score += target.level * ally.atk;
                            if (ally.atk >= target.hp)
                                score += target.level * target.hp;
                            if (extra.target == "my_miracle")
                                score += 50;
                            else if (extra.target == "my_barrack")
                                score += 20;
                            if (extra.type == "attack")
                                score += 5;
                            if (canAttack(target, ally)) //敌人能打到我
                            {
                                score -= ally.level * target.atk;
                                if (target.atk >= ally.hp)
                                    score -= ally.level * ally.hp;
                                if (extra.type == "avoid")
                                    score -= 50;
                            }
                            if (max_score < score)
                            {
                                max_score = score;
                                tar = target.id;
                            }
                        }
                        if (tar != -1)
                        {
                            unit_extra_info[ally.id].type = "attack";
                            unit_extra_info[ally.id].attack_id = tar;
                        }
                    }
                    else if (ally.type == "Archer")
                    {
                        nth_element(enemy_list.begin(), enemy_list.begin(), enemy_list.end(),
                                    [](const Unit &_enemy1, const Unit &_enemy2) { return _enemy1.atk < _enemy2.atk; });
                        unit_extra_info[ally.id].type = "attack";
                        unit_extra_info[ally.id].target = target_list[0].id;
                    }
                    else if (ally.type == "Priest")
                    {
                        int max_score = -1;
                        int tar = -1;
                        for (auto target : target_list)
                        {
                            int score = 0;
                            struct unit_info &extra = unit_extra_info[target.id];
                            score += target.level * ally.atk;
                            if (ally.atk >= target.hp)
                                score += target.level * target.hp;
                            if (extra.target == "my_miracle")
                                score += 50;
                            else if (extra.target == "my_barrack")
                                score += 20;
                            if (extra.type == "attack")
                                score += 5;
                            if (canAttack(target, ally)) //敌人能打到我
                            {
                                score -= ally.level * target.atk;
                                if (target.atk >= ally.hp)
                                    score -= ally.level * ally.hp;
                                if (extra.type == "avoid")
                                    score -= 50;
                            }
                            if (max_score < score)
                            {
                                max_score = score;
                                tar = target.id;
                            }
                        }
                        if (tar != -1)
                        {
                            unit_extra_info[ally.id].type = "attack";
                            unit_extra_info[ally.id].target = tar;
                        }
                    }
                }
            }
                

            
        }
    }*/
    
}

void AI::march_after_battle(const string &type)
{

}

void AI::battle(const string &type)
{
    //处理生物的战斗

    /*
     * 基本思路，行动顺序:
     * 火山龙：攻击高>低 （大AOE输出），随机攻击
     * 剑士：攻击低>高 打消耗，优先打攻击力低的
     * 弓箭手：攻击高>低 优先打不能反击的攻击力最高的，其次打能反击的攻击力最低的
     * 对单位的战斗完成后，对神迹进行输出
     * 【进阶】对战斗范围内敌方目标的价值进行评估，通过一些匹配算法决定最优的战斗方式
     * 例如占领着驻扎点的敌方生物具有极高的价值，优先摧毁可以使敌方下回合损失很多可用出兵点
     * 一些生物不攻击而移动，或完全不动，可能能带来更大的威慑力，而赢得更多优势
     */

    auto ally_list = getUnitsByCamp(my_camp);

    //自定义排列顺序
    auto cmp = [](const Unit &unit1, const Unit &unit2) {
        if (unit1.can_atk != unit2.can_atk) //首先要能动
            return unit2.can_atk < unit1.can_atk;
        else if (unit1.type != unit2.type)
        { //火山龙>剑士>弓箭手
            auto type_id_gen = [](const string &type_name) {
                if (type_name == "VolcanoDragon")
                    return 0;
                else if (type_name == "Swordsman")
                    return 1;
                else
                    return 2;
            };
            return (type_id_gen(unit1.type) < type_id_gen(unit2.type));
        }
        else if (unit1.type == "VolcanoDragon" or unit1.type == "Archer")
            return unit2.atk < unit1.atk;
        else
            return unit1.atk < unit2.atk;
    };
    //按顺序排列好单位，依次攻击
    sort(ally_list.begin(), ally_list.end(), cmp);
    for (const auto &ally : ally_list)
    {
        if (!ally.can_atk)
            break;
        auto enemy_list = getUnitsByCamp(my_camp ^ 1);
        vector<Unit> target_list;
        for (const auto &enemy : enemy_list)
            if (AiClient::canAttack(ally, enemy))
                target_list.push_back(enemy);
        if (target_list.empty())
            continue;
        if (ally.type == "VolcanoDragon")
        {
            default_random_engine g(static_cast<unsigned int>(time(nullptr)));
            int tar = uniform_int_distribution<>(0, target_list.size() - 1)(g);
            attack(ally.id, target_list[tar].id);
        }
        else if (ally.type == "Swordsman")
        {
            nth_element(enemy_list.begin(), enemy_list.begin(), enemy_list.end(),
                        [](const Unit &_enemy1, const Unit &_enemy2) { return _enemy1.atk < _enemy2.atk; });
            attack(ally.id, target_list[0].id);
        }
        else if (ally.type == "Archer")
        {
            sort(enemy_list.begin(), enemy_list.end(),
                 [](const Unit &_enemy1, const Unit &_enemy2) { return _enemy1.atk > _enemy2.atk; });
            bool suc = false;
            for (const auto &enemy : target_list)
                if (!canAttack(enemy, ally))
                {
                    attack(ally.id, enemy.id);
                    suc = true;
                    break;
                }
            if (suc)
                continue;
            nth_element(enemy_list.begin(), enemy_list.begin(), enemy_list.end(),
                        [](const Unit &_enemy1, const Unit &_enemy2) { return _enemy1.atk < _enemy2.atk; });
            attack(ally.id, target_list[0].id);
        }
    }
    //最后攻击神迹
    ally_list = getUnitsByCamp(my_camp);
    sort(ally_list.begin(), ally_list.end(), cmp);
    for (auto ally : ally_list)
    {
        if (!ally.can_atk)
            break;
        int dis = cube_distance(ally.pos, enemy_miracle_pos);
        if (ally.atk_range[0] <= dis && dis <= ally.atk_range[1])
            attack(ally.id, my_camp ^ 1);
    }
}

void AI::march_after_battle(const string &type)
{
    //处理生物的移动

    /*
     * 先动所有剑士，尽可能向敌方神迹移动
     * 若目标驻扎点上没有地面单位，则让弓箭手向目标驻扎点移动，否则尽可能向敌方神迹移动
     * 然后若目标驻扎点上没有地面单位，则让火山之龙向目标驻扎点移动，否则尽可能向敌方神迹移动
     * 【进阶】一味向敌方神迹移动并不一定是个好主意
     * 在移动的时候可以考虑一下避开敌方生物攻击范围实现、为己方强力生物让路、堵住敌方出兵点等策略
     * 如果采用其他生物组合，可以考虑抢占更多驻扎点
     */
    auto ally_list = getUnitsByCamp(my_camp);
    sort(ally_list.begin(), ally_list.end(), [](const Unit &_unit1, const Unit &_unit2) {
        auto type_id_gen = [](const string &type_name) {
            if (type_name == "Swordsman")
                return 0;
            else if (type_name == "Archer")
                return 1;
            else
                return 2;
        };
        return type_id_gen(_unit1.type) < type_id_gen(_unit2.type);
    });
    for (const auto &ally : ally_list)
    {
        if (!ally.can_move)
            continue;
        if (ally.type == "Swordsman")
        {
            //获取所有可到达的位置
            auto reach_pos_with_dis = reachable(ally, map);
            //压平
            vector<Pos> reach_pos_list;
            for (const auto &reach_pos : reach_pos_with_dis)
            {
                for (auto pos : reach_pos)
                    reach_pos_list.push_back(pos);
            }
            if (reach_pos_list.empty())
                continue;
            //优先走到距离敌方神迹更近的位置
            nth_element(reach_pos_list.begin(), reach_pos_list.begin(), reach_pos_list.end(),
                        [this](Pos _pos1, Pos _pos2) {
                            return cube_distance(_pos1, enemy_miracle_pos) < cube_distance(_pos2, enemy_miracle_pos);
                        });
            move(ally.id, reach_pos_list[0]);
        }
        else
        {
            //如果已经在兵营就不动了
            bool on_barrack = false;
            for (const auto &barrack : map.barracks)
                if (ally.pos == barrack.pos)
                {
                    on_barrack = true;
                    break;
                }
            if (on_barrack)
                continue;

            //获取所有可到达的位置
            auto reach_pos_with_dis = reachable(ally, map);
            //压平
            vector<Pos> reach_pos_list;
            for (const auto &reach_pos : reach_pos_with_dis)
            {
                for (auto pos : reach_pos)
                    reach_pos_list.push_back(pos);
            }
            if (reach_pos_list.empty())
                continue;

            //优先走到未被占领的兵营，否则走到
            if (getUnitByPos(target_barrack, false).id == -1)
            {
                nth_element(reach_pos_list.begin(), reach_pos_list.begin(), reach_pos_list.end(),
                            [this](Pos _pos1, Pos _pos2) {
                                return cube_distance(_pos1, target_barrack) < cube_distance(_pos2, target_barrack);
                            });
                move(ally.id, reach_pos_list[0]);
            }
            else
            {
                nth_element(reach_pos_list.begin(), reach_pos_list.begin(), reach_pos_list.end(),
                            [this](Pos _pos1, Pos _pos2) {
                                return cube_distance(_pos1, enemy_miracle_pos) < cube_distance(_pos2, enemy_miracle_pos);
                            });
                move(ally.id, reach_pos_list[0]);
            }
        }
    }
}

void AI::creat_unit(const string &type)
{
    //最后进行召唤
    if(type == "attack")
    {
        //将所有本方出兵点按照到对方基地的距离排序，从近到远出兵
        auto summon_pos_list = getSummonPosByCamp(my_camp);
        sort(summon_pos_list.begin(), summon_pos_list.end(), [this](Pos _pos1, Pos _pos2) {
            return cube_distance(_pos1, enemy_miracle_pos) < cube_distance(_pos2, enemy_miracle_pos);
        });
        vector<Pos> available_summon_pos_list;
        for (auto pos : summon_pos_list)
        {
            auto unit_on_pos_ground = getUnitByPos(pos, false);
            if (unit_on_pos_ground.id == -1)
                available_summon_pos_list.push_back(pos);
        }

        //统计各个生物的可用数量，在假设出兵点无限的情况下，按照1个剑士、1个弓箭手、1个火山龙的顺序召唤
        int mana = players[my_camp].mana;
        auto deck = players[my_camp].creature_capacity;
        ::map<string, int> available_count;
        for (const auto &card_unit : deck)
            available_count[card_unit.type] = card_unit.available_count;

        vector<string> summon_list;
        //剑士和弓箭手数量不足或者格子不足则召唤火山龙
        if ((available_summon_pos_list.size() == 1 || available_count["Swordsman"] + available_count["Archer"] < 2) &&
            mana >= CARD_DICT.at("VolcanoDragon")[1].cost && available_count["VolcanoDragon"] > 0)
        {
            summon_list.emplace_back("VolcanoDragon");
            mana -= CARD_DICT.at("VolcanoDragon")[1].cost;
        }

        bool suc = true;
        while (mana >= 2 && suc)
        {
            suc = false;
            if (available_count["Swordsman"] > 0 && mana >= CARD_DICT.at("Swordsman")[1].cost)
            {
                summon_list.emplace_back("Swordsman");
                mana -= CARD_DICT.at("Swordsman")[1].cost;
                available_count["Swordsman"] -= 1;
                suc = true;
            }
            if (available_count["Archer"] > 0 && mana >= CARD_DICT.at("Archer")[1].cost)
            {
                summon_list.emplace_back("Archer");
                mana -= CARD_DICT.at("Archer")[1].cost;
                available_count["Archer"] -= 1;
                suc = true;
            }
            if (available_count["VolcanoDragon"] > 0 && mana >= CARD_DICT.at("VolcanoDragon")[1].cost)
            {
                summon_list.emplace_back("VolcanoDragon");
                mana -= CARD_DICT.at("VolcanoDragon")[1].cost;
                available_count["VolcanoDragon"] -= 1;
                suc = true;
            }
        }

        int i = 0;
        for (auto pos : available_summon_pos_list)
        {
            if (i == summon_list.size())
                break;
            summon(summon_list[i], 1, pos);
            ++i;
        }
    }
}

Pos AI::counter(vector<Pos>& pos_list, string type, int number, int range)
{
    std::map<Pos, int> map_counter;
    if (type == "most_enermy")
    {
        for (auto pos : pos_list)
        {
            auto unit_list = units_in_range(pos, range, map, my_camp^1);
            map_counter[pos] = unit_list.size();
        }
    }

    else if (type == "inferno_flame")
    {
        for (auto pos : pos_list)
        {
            int score = 0;
            auto unit_list = units_in_range(pos, range, map, my_camp ^ 1);
            for(auto unit:unit_list)
            {
                if (unit.hp <= number)
                    score += unit.level * ENEMY_DEATH;
                else
                    score += number * ENEMY_HURT;
            }
            score += (20 - cube_distance(pos, enemy_miracle_pos)) * ENEMY_MIRCLE;
            map_counter[pos] = score;
        }
    }

    auto best_pos = pos_list[0];
    int max_benefit = 0;
    for (auto pos : pos_list)
    {
        if (map_counter[pos] > max_benefit)
        {
            best_pos = pos;
            max_benefit = map_counter[pos];
        }
    }

    return best_pos;
}

Pos AI::counter(vector<Pos> &pos_list, string type, Unit my_unit)
{
    std::map<Pos, int> map_counter;
    struct unit_info &my_extra = unit_extra_info[my_unit.id];

    if (type == "attack")
    {
        for (auto pos : pos_list)
        {
            int score = 0;
            if (my_extra.type == "move" && pos == my_extra.destination)
            {
                my_extra.type = "attack";
                return my_extra.destination;
            }
            auto unit_list = units_in_range(pos, 4, map, my_camp ^ 1);
            for (auto unit : unit_list)
            {
                struct unit_info &extra = unit_extra_info[unit.id];
                if (canAttack(my_unit, unit)) //我能打到敌人
                {
                    score += unit.level * my_unit.atk;
                    if (my_unit.atk >= unit.hp)
                        score += unit.level * my_unit.atk;
                    if (extra.target == "my_miracle")
                        score += 30;
                    if (extra.target == "my_barrack")
                        score += 15;
                    if (extra.type == "attack")
                        score += 5;
                }
                if (canAttack(unit, my_unit)) //敌人能打到我
                {
                    score -= my_unit.level * unit.atk;
                    if (unit.atk >= my_unit.hp)
                        score -= my_unit.level * my_unit.hp;
                    if (extra.type == "avoid")
                        score -= 30;
                }
            }
            score += (20 - cube_distance(pos, enemy_miracle_pos)) * ENEMY_MIRCLE;
            if (checkBarrack(enemy_barrack) != my_camp)
                score += (20 - cube_distance(pos, enemy_barrack)) * ENEMY_BARRACK;
            map_counter[pos] = score;
        }
    }
    else if (type == "waste")
    {
        for (auto pos : pos_list)
        {
            int score = 0;
            if (my_extra.type == "move" && pos == my_extra.destination)
            {
                my_extra.type = "attack";
                return my_extra.destination;
            }
            if (checkBarrack(enemy_barrack)!=my_camp)
                score += (20 - cube_distance(pos, enemy_barrack)) * ENEMY_BARRACK;
            score += (20 - cube_distance(pos, enemy_miracle_pos)) * ENEMY_MIRCLE; //冲就是了         
            map_counter[pos] = score;
        }
    }

    vector<Pos> & enemy_summon= getSummonPosByCamp(my_camp^1);

    for (auto pos : enemy_summon)  //占用对方出兵点
    {
        if(map_counter.find(pos)!=map_counter.end())
            map_counter[pos] += 10;
    }

    auto best_pos = pos_list[0];
    int max_benefit = 0;
    for (auto pos : pos_list)
    {
        if (map_counter[pos] > max_benefit)
        {
            best_pos = pos;
            max_benefit = map_counter[pos];
        }
    }

    return best_pos;
}

Pos AI::posShift(Pos pos, string direct,const int& lenth = 1)
{
    /*
     * 对于给定位置，给出按照自己的视角（神迹在最下方）的某个方向移动一步后的位置
     * 本段代码可以自由取用
     * @param pos:  (x, y, z)
     * @param direct: 一个str，含2个字符，意义见注释
     * @return: 移动后的位置 (x', y', z')
     */
    transform(direct.begin(), direct.end(), direct.begin(), ::toupper);
    if (my_camp == 0)
    {
        if (direct == "FF") //正前方
            return make_tuple(get<0>(pos) + lenth, get<1>(pos) - lenth, get<2>(pos));
        else if (direct == "SF") //优势路前方（自身视角右侧为优势路）
            return make_tuple(get<0>(pos) + lenth, get<1>(pos), get<2>(pos) - lenth);
        else if (direct == "IF") //劣势路前方
            return make_tuple(get<0>(pos), get<1>(pos) - lenth, get<2>(pos) + lenth);
        else if (direct == "BB") //正后方
            return make_tuple(get<0>(pos) - lenth, get<1>(pos) + lenth, get<2>(pos));
        else if (direct == "SB") //优势路后方
            return make_tuple(get<0>(pos), get<1>(pos) + lenth, get<2>(pos) - lenth);
        else if (direct == "IB") //劣势路后方
            return make_tuple(get<0>(pos) - lenth, get<1>(pos), get<2>(pos) + lenth);
    }
    else
    {
        if (direct == "FF") //正前方
            return make_tuple(get<0>(pos) - lenth, get<1>(pos) + lenth, get<2>(pos));
        else if (direct == "SF") //优势路前方（自身视角右侧为优势路）
            return make_tuple(get<0>(pos) - lenth, get<1>(pos), get<2>(pos) + lenth);
        else if (direct == "IF") //劣势路前方
            return make_tuple(get<0>(pos), get<1>(pos) + lenth, get<2>(pos) - lenth);
        else if (direct == "BB") //正后方
            return make_tuple(get<0>(pos) + lenth, get<1>(pos) - lenth, get<2>(pos));
        else if (direct == "SB") //优势路后方
            return make_tuple(get<0>(pos), get<1>(pos) - lenth, get<2>(pos) + lenth);
        else if (direct == "IB") //劣势路后方
            return make_tuple(get<0>(pos) + lenth, get<1>(pos), get<2>(pos) - lenth);
    }
}

void AI::use_inferno()
{
    if (players[my_camp].mana >= 8 && players[my_camp].artifact[0].state == "Ready")
    {
        auto pos_list = all_pos_in_map();
        vector<Pos> postions;
        for (auto pos : pos_list)
        {
            if (canUseArtifact(players[my_camp].artifact[0], pos, my_camp))
                postions.push_back(pos);
        }
        Pos decide_pos = counter(postions, "inferno_flame", 2, 2);
        use(players[my_camp].artifact[0].id, decide_pos);
    }
}

int main()
{
    std::ifstream datafile("Data.json");
    json all_data;
    datafile >> all_data;
    card::get_data_from_json(all_data);

    AI player_ai;
    player_ai.chooseCards();
    while (true)
    {
        player_ai.updateGameInfo();
        player_ai.play();
    }
    return 0;
}
