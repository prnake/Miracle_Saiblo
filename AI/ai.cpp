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
using std::list;
using std::make_tuple;
using std::map;
using std::string;
using std::uniform_int_distribution;
using std::vector;

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
    int creat_time; //创建时间
    int priority;
    std::list<Pos> history; //历史路径
    Pos now;
    string target; //预测行为
    string type;   //分配对策
    int des;       //我方单位目标 des=1,占领destination;des=2,直线进攻基地;des=3，保护神迹；des=4,保护我方驻扎点；des=5，保护对方驻扎点
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

    Pos counter(vector<Pos> &pos_list, string type);

    Pos counter(vector<Pos> &pos_list, string type, Unit &my_unit);

    int which_to_attack(string type, Unit &ally);

    void sign_unit(int d);

    void sign_unit(int d, const Pos &dest);

public:
    AI()
    {
        unit_extra_info.resize(2000);
    }
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

void AI::sign_unit(int d)
{
    int id = map.units[map.units.size() - 1].id;
    unit_extra_info[id].des = d;
    unit_extra_info[id].creat_time = round;
}

void AI::sign_unit(int d, const Pos &dest)
{
    int id = map.units[map.units.size() - 1].id;
    unit_extra_info[id].des = d;
    unit_extra_info[id].destination = dest;
    unit_extra_info[id].creat_time = round;
}

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
        if (my_camp == 0)
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

        /*
        for (const auto &barrack : map.barracks)
        {
            if (cube_distance(miracle_pos, barrack.pos) <
                cube_distance(miracle_pos, target_barrack))
                target_barrack = barrack.pos;
        }
        */

        // 在正中心偏右召唤一个牧师，用来抢占驻扎点
        summon("Priest", 1, posShift(miracle_pos, "SF", 1));
        sign_unit(1, my_barrack);
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

    auto enemy_list = getUnitsByCamp(my_camp ^ 1);

    for (const auto &enemy : enemy_list)
    {
        struct unit_info &extra = unit_extra_info.at(enemy.id);
        if (extra.creat_time == -1)
        {
            extra.creat_time = round;
            extra.now = enemy.pos;
            if (enemy.type == "Archer")
            {
                extra.type = "attack";
                extra.priority = enemy.atk;
            }
            else if (enemy.type == "Swordsman")
            {
                extra.type = "attack";
                extra.priority = enemy.atk;
            }
            else if (enemy.type == "BlackBat")
            {
                extra.type = "attack";
                extra.priority = enemy.atk;
            }
            else if (enemy.type == "Priest")
            {
                extra.type = "attack";
                if (enemy.level == 1)
                {
                    auto atk_up_enemy = units_in_range(extra.now, 2, map, my_camp ^ 1);
                    extra.priority = atk_up_enemy.size() - 1;
                }
                else
                {
                    auto atk_up_enemy = units_in_range(extra.now, 3, map, my_camp ^ 1);
                    extra.priority += atk_up_enemy.size() - 1;
                }
            }
            else if (enemy.type == "VolcanoDragon")
            {
                extra.type = "attack";
                extra.priority = enemy.atk;
            }
            else if (enemy.type == "Inferno")
            {
                if (cube_distance(extra.now, miracle_pos) <= 3)
                {
                    extra.type = "attack";
                    extra.target = "my_miracle";
                    extra.priority = enemy.atk;
                }
                else
                {
                    extra.type = "avoid";
                    extra.priority = 2;
                }
            }
            else if (enemy.type == "FrostDragon")
            {
                extra.type = "attack";
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
                if (min > now_distance)
                {
                    min = now_distance[i];
                    closest = i;
                }
            }
            if (min <= enemy.atk_range[1] + enemy.max_move)
            {
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
            }
        }
    }

    auto ally_list = getUnitsByCamp(my_camp);
    for (const auto &ally : ally_list)
    {
        struct unit_info &ally_extra = unit_extra_info[ally.id];
        //我方单位目标 des=1,占领destination;des=2,直线进攻基地;des=3，保护神迹；des=4,保护我方驻扎点；des=5，保护对方驻扎点
        if (ally_extra.des == 1)
        {
            Unit pos_unit = getUnitByPos(ally_extra.destination);
            if (pos_unit.id != -1 && pos_unit.camp == my_camp)
            {
                ally_extra.des = 0;
            }
        }

        else if (ally_extra.des == 2)
        {
            //do_nothing
        }

        else if (ally_extra.des == 3)
        {
            //do_nothing
        }

        else if (ally_extra.des == 4)
        {
            if (checkBarrack(my_barrack) != my_camp)
            {
                ally_extra.des = 1;
                ally_extra.destination = my_barrack;
            }
        }

        else if (ally_extra.des == 5)
        {
            if (checkBarrack(enemy_barrack) != my_camp)
            {
                ally_extra.des = 1;
                ally_extra.destination = enemy_barrack;
            }
        }
    }
}

void AI::march_before_battle(const string &type) //处理不得不移动的部分——占点/冲塔；简单策略：能占到目标点/被不能被攻击到的人打掉的血量最小且小于HP
{
    if (type == "attack")
    {
        auto ally_list = getUnitsByCamp(my_camp);
        auto enemy_list = getUnitsByCamp(my_camp ^ 1);
        auto cmp = [](const Unit &unit1, const Unit &unit2) {
            if (unit1.can_move != unit2.can_move) //首先要能动
                return unit2.can_move < unit1.can_move;
            else if (unit1.type != unit2.type)
            { //地狱犬>冰龙>弓箭手>牧师
                auto type_id_gen = [](const string &type_name) {
                    if (type_name == "Inferno")
                        return 0;
                    if (type_name == "FrostDragon")
                        return 1;
                    else if (type_name == "Archer")
                        return 2;
                    else
                        return 3;
                };
                return (type_id_gen(unit1.type) < type_id_gen(unit2.type));
            }
            else
                return unit2.atk < unit1.atk;
        };

        sort(ally_list.begin(), ally_list.end(), cmp);

        for (const auto &ally : ally_list)
        {
            if (!ally.can_move)
                break;
            struct unit_info &ally_extra = unit_extra_info[ally.id];
            Pos decide_pos = miracle_pos;

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
            {
                ally_extra.type = "can_not_move";
                continue;
            }
            else
            {
                //我方单位目标 des=1,占领destination;des=2,直线进攻基地;des=3，保护神迹；des=4,保护我方驻扎点；des=5，保护对方驻扎点
                if (ally_extra.des == 1)
                {
                    decide_pos = counter(reach_pos_list, "close", ally);
                }

                else if (ally_extra.des == 2)
                {
                    decide_pos = counter(reach_pos_list, "atk_miracle", ally);
                }

                else if (ally_extra.des == 3)
                {
                    decide_pos = counter(reach_pos_list, "protect_miracle", ally);
                }

                else if (ally_extra.des == 4)
                {
                    decide_pos = counter(reach_pos_list, "protect_my_barrack", ally);
                }

                else if (ally_extra.des == 5)
                {
                    decide_pos = counter(reach_pos_list, "protect_enemy_barrack", ally);
                }
            }

            if (decide_pos != miracle_pos && decide_pos != ally.pos)
                move(ally.id, decide_pos);
        }
    }
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

void AI::battle(const string &type)
{
    if (type == "attack")
    {
        auto ally_list = getUnitsByCamp(my_camp);
        auto enemy_list = getUnitsByCamp(my_camp ^ 1);

        //这里直接从攻击力大到小排序
        auto cmp = [](const Unit &unit1, const Unit &unit2) {
            if (unit1.can_atk != unit2.can_atk) //首先要能打
                return unit2.can_atk < unit1.can_atk;
            else if (unit2.atk == unit1.atk)
                return unit2.max_hp < unit1.max_hp; //攻击力相同优先血量高的（好像不能算距离）
            else
                return unit2.atk < unit1.atk； //优先攻击力高的
        };

        sort(ally_list.begin(), ally_list.end(), cmp);

        for (const auto &ally : ally_list)
        {
            if (!ally.can_atk)
                break;
            struct unit_info &ally_extra = unit_extra_info[ally.id];
            int attack_id = -1;
            if (ally_extra.type == "can_not_move")
            {
                attack_id = which_to_attack("atk_anyway", ally);
            }
            else if (ally_extra.des == 1)
            {
                attack_id = which_to_attack("close", ally);
            }

            else if (ally_extra.des == 2)
            {
                attack_id = which_to_attack("atk_miracle", ally);
            }

            else if (ally_extra.des == 3)
            {
                attack_id = which_to_attack("protect_miracle", ally);
            }

            else if (ally_extra.des == 4)
            {
                attack_id = which_to_attack("protect_my_barrack", ally);
            }

            else if (ally_extra.des == 5)
            {
                attack_id = which_to_attack("protect_enemy_barrack", ally);
            }
            else
            {
                attack_id = which_to_attack("attack", ally);
            }
            if (attack_id != -1)
                attack(ally.id, attack_id);
        }
    }
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

    /*
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
    */
}

void AI::march_after_battle(const string &type)
{
    if (type == "attack")
    {
        auto ally_list = getUnitsByCamp(my_camp);
        auto enemy_list = getUnitsByCamp(my_camp ^ 1);
        auto cmp = [](const Unit &unit1, const Unit &unit2) {
            if (unit1.can_move != unit2.can_move) //首先要能动
                return unit2.can_move < unit1.can_move;
            else if (unit1.type != unit2.type)
            { //地狱犬>冰龙>弓箭手>牧师
                auto type_id_gen = [](const string &type_name) {
                    if (type_name == "Inferno")
                        return 0;
                    if (type_name == "FrostDragon")
                        return 1;
                    else if (type_name == "Archer")
                        return 2;
                    else
                        return 3;
                };
                return (type_id_gen(unit1.type) < type_id_gen(unit2.type));
            }
            else
                return unit2.atk < unit1.atk;
        };

        sort(ally_list.begin(), ally_list.end(), cmp);

        for (const auto &ally : ally_list)
        {
            if (!ally.can_move)
                break;
            struct unit_info &ally_extra = unit_extra_info[ally.id];
            Pos decide_pos = miracle_pos;

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
            {
                ally_extra.type = "can_not_move";
                continue;
            }
            else
            {
                decide_pos = counter(reach_pos_list, "attack", ally);
            }

            if (decide_pos != miracle_pos && decide_pos != ally.pos)
                move(ally.id, decide_pos);
        }
    }
    //处理生物的移动

    /*
     * 先动所有剑士，尽可能向敌方神迹移动
     * 若目标驻扎点上没有地面单位，则让弓箭手向目标驻扎点移动，否则尽可能向敌方神迹移动
     * 然后若目标驻扎点上没有地面单位，则让火山之龙向目标驻扎点移动，否则尽可能向敌方神迹移动
     * 【进阶】一味向敌方神迹移动并不一定是个好主意
     * 在移动的时候可以考虑一下避开敌方生物攻击范围实现、为己方强力生物让路、堵住敌方出兵点等策略
     * 如果采用其他生物组合，可以考虑抢占更多驻扎点
     */
    /*
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
    */
}

void AI::creat_unit(const string &type)
{
    //最后进行召唤
    if (type == "attack")
    {
        //统计各个生物的可用数量，在假设出兵点无限的情况下，按照1个剑士、1个弓箭手、1个火山龙的顺序召唤
        int mana = players[my_camp].mana;
        auto deck = players[my_camp].creature_capacity;
        auto ally_list = getUnitsByCamp(my_camp);
        auto enemy_list = getUnitsByCamp(my_camp ^ 1);

        ::map<string, int> available_count;
        int sum_count = 0;
        for (const auto &card_unit : deck)
        {
            available_count[card_unit.type] = card_unit.available_count;
            sum_count += card_unit.available_count;
        }

        //送塔设计
        if (sum_count < 3)
        {
            auto cmp = [](const Unit &unit1, const Unit &unit2) {
                return unit2.id < unit1.id;
            };

            sort(ally_list.begin(), ally_list.end(), cmp);

            for (int i = sum_count; i <= 3; i++) //保留三张单位,卖掉最初的单位
            {
                struct unit_info &ally_extra = unit_extra_info[ally_list[3 - i].id];
                ally_extra.des = 2;
            }
        }

        //顺序：保护神迹->占领我方出兵点->占领对面出兵点->攻击对面神迹，简化为出兵点排序顺序和任务指派
        int priority_my_miracle = 0, priority_my_barrack = 0, priority_enemy_barrack = 0, priority_enemy_miracle = 0;
        auto summon_pos_list = getSummonPosByCamp(my_camp);
        vector<Pos> available_summon_pos_list;
        int des = 0;
        Pos destination = miracle_pos;

        //扫描我方神迹附近威胁
        for (auto enemy : enemy_list)
        {
            struct unit_info &enemy_extra = unit_extra_info[enemy.id];
            if (enemy_extra.target == "my_miracle")
                priority_my_miracle += enemy_extra.priority;
            else if (enemy_extra.target == "my_barrack")
                priority_my_barrack += enemy_extra.priority;
            else if (enemy_extra.target == "enemy_barrack")
                priority_enemy_barrack += enemy_extra.priority;
            else if (enemy_extra.target == "enemy_miracle")
                priority_enemy_miracle += enemy_extra.priority;
        }

        for (auto ally : ally_list)
        {
            struct unit_info &ally_extra = unit_extra_info[ally.id];
            if (ally_extra.des == 3)
                priority_my_miracle -= ally.atk * 2;
            else if (ally_extra.des == 4)
                priority_my_barrack -= ally.atk * 2;
            else if (ally_extra.des == 5)
                priority_enemy_barrack -= ally.atk * 2;
            else if (ally_extra.des == 2)
                priority_enemy_miracle -= ally.atk * 2;
        }
        //我方单位目标 des=1,占领destination;des=2,直线进攻基地;des=3，保护神迹；des=4,保护我方驻扎点；des=5，保护对方驻扎点
        if (priority_my_miracle > 0) //基地有危险
        {
            //将所有本方出兵点按照到我方神迹的距离排序，从近到远出兵
            sort(summon_pos_list.begin(), summon_pos_list.end(), [this](Pos _pos1, Pos _pos2) {
                return cube_distance(_pos1, miracle_pos) < cube_distance(_pos2, miracle_pos);
            });
            for (auto pos : summon_pos_list)
            {
                auto unit_on_pos_ground = getUnitByPos(pos, false);
                if (unit_on_pos_ground.id == -1)
                    available_summon_pos_list.push_back(pos);
            }
            des = 3;
        }

        else if (checkBarrack(my_barrack) != my_camp) //召唤生物占领我方出兵点
        {
            //将所有本方出兵点按照到我方出兵点的距离排序，从近到远出兵
            sort(summon_pos_list.begin(), summon_pos_list.end(), [this](Pos _pos1, Pos _pos2) {
                return cube_distance(_pos1, my_barrack) < cube_distance(_pos2, my_barrack);
            });
            for (auto pos : summon_pos_list)
            {
                auto unit_on_pos_ground = getUnitByPos(pos, false);
                if (unit_on_pos_ground.id == -1)
                    available_summon_pos_list.push_back(pos);
            }
            des = 1;
            destination = my_barrack;
        }

        else if (checkBarrack(my_barrack) == my_camp && priority_my_barrack > 0) //召唤生物占领我方出兵点
        {
            //将所有本方出兵点按照到我方出兵点的距离排序，从近到远出兵
            sort(summon_pos_list.begin(), summon_pos_list.end(), [this](Pos _pos1, Pos _pos2) {
                return cube_distance(_pos1, my_barrack) < cube_distance(_pos2, my_barrack);
            });
            for (auto pos : summon_pos_list)
            {
                auto unit_on_pos_ground = getUnitByPos(pos, false);
                if (unit_on_pos_ground.id == -1)
                    available_summon_pos_list.push_back(pos);
            }
            des = 4;
        }

        else if (checkBarrack(enemy_barrack) != my_camp) //召唤生物占领敌方出兵点
        {
            //将所有本方出兵点按照到对方出兵点的距离排序，从近到远出兵
            sort(summon_pos_list.begin(), summon_pos_list.end(), [this](Pos _pos1, Pos _pos2) {
                return cube_distance(_pos1, enemy_barrack) < cube_distance(_pos2, enemy_barrack);
            });
            for (auto pos : summon_pos_list)
            {
                auto unit_on_pos_ground = getUnitByPos(pos, false);
                if (unit_on_pos_ground.id == -1)
                    available_summon_pos_list.push_back(pos);
            }
            des = 1;
            destination = enemy_barrack;
        }

        else if (checkBarrack(enemy_barrack) == my_camp && priority_enemy_barrack > 0) //召唤生物占领敌方出兵点
        {
            //将所有本方出兵点按照到对方出兵点的距离排序，从近到远出兵
            sort(summon_pos_list.begin(), summon_pos_list.end(), [this](Pos _pos1, Pos _pos2) {
                return cube_distance(_pos1, enemy_barrack) < cube_distance(_pos2, enemy_barrack);
            });
            for (auto pos : summon_pos_list)
            {
                auto unit_on_pos_ground = getUnitByPos(pos, false);
                if (unit_on_pos_ground.id == -1)
                    available_summon_pos_list.push_back(pos);
            }
            des = 5;
        }

        //无事可做就进攻对面神迹
        else
        {
            //将所有本方出兵点按照到对方神迹的距离排序，从近到远出兵
            sort(summon_pos_list.begin(), summon_pos_list.end(), [this](Pos _pos1, Pos _pos2) {
                return cube_distance(_pos1, enemy_miracle_pos) < cube_distance(_pos2, enemy_miracle_pos);
            });
            for (auto pos : summon_pos_list)
            {
                auto unit_on_pos_ground = getUnitByPos(pos, false);
                if (unit_on_pos_ground.id == -1)
                    available_summon_pos_list.push_back(pos);
            }
            des = 0;
        }

        vector<string> summon_list;
        vector<int> summon_list_level;
        //等级从高到低
        for (int i = 3; i >= 1; i--)
        {
            int flag = 0;
            //my_creatures = {"Archer", "Priest", "FrostDragon"};
            //优先召唤牧师
            if (available_count["Priest"] > 0 && mana >= CARD_DICT.at("Priest")[i].cost)
            {
                summon_list.emplace_back("Priest");
                summon_list_level.emplace_back(i);
                mana -= CARD_DICT.at("Priest")[i].cost;
                available_count["Priest"] -= 1;
                flag = 1;
            }
            //其次是冰龙
            if (available_count["FrostDragon"] > 0 && mana >= CARD_DICT.at("FrostDragon")[i].cost)
            {
                summon_list.emplace_back("FrostDragon");
                summon_list_level.emplace_back(i);
                mana -= CARD_DICT.at("FrostDragon")[i].cost;
                available_count["FrostDragon"] -= 1;
                flag = 1;
            }
            //最后是弓箭手
            if (available_count["Archer"] > 0 && mana >= CARD_DICT.at("Archer")[i].cost)
            {
                summon_list.emplace_back("Archer");
                summon_list_level.emplace_back(i);
                mana -= CARD_DICT.at("Archer")[i].cost;
                available_count["Archer"] -= 1;
                flag = 1;
            }
            if (flag)
                i++;
        }

        int i = 0;
        for (auto pos : available_summon_pos_list)
        {
            if (i == summon_list.size())
                break;
            summon(summon_list[i], summon_list_level[i], pos);
            if (destination != miracle_pos)
                sign_unit(des, destination);
            else
                sign_unit(des);
            ++i;
        }
    }
}

int AI::which_to_attack(string type, Unit &ally)
{
    auto enemy_list = getUnitsByCamp(my_camp ^ 1);
    vector<Unit> target_list;
    std::map<int, int> target_id_counter;
    int health_decrease = 0, de_benefit = 0;
    int best_target = -1;
    int max_benefit = -1;

    if (type == "close")
        max_benefit = 5;
    else if (type == "atk_miracle")
    {
        int dis = cube_distance(ally.pos, enemy_miracle_pos);
        if (ally.atk_range[0] <= dis && dis <= ally.atk_range[1])
        {
            return my_camp ^ 1;
        }
        else
            max_benefit = 10;
    }
    else if (type == "atk_anyway")
    {
        max_benefit = -9999;
    }

    for (const auto &enemy : enemy_list)
    {
        if (AiClient::canAttack(ally, enemy))
        {
            target_id_counter[enemy.id] = 0;
            target_list.push_back(enemy);

            int benefit = 0;
            int health = ally.hp;
            struct unit_info &enemy_extra = unit_extra_info[enemy.id];

            if (type == "protect_miracle")
            {
                int dis = cube_distance(enemy.pos, miracle_pos);
                if (enemy.atk_range[0] <= dis && dis <= enemy.atk_range[1])
                {
                    return enemy.id;
                }
                else if (dis <= enemy.atk_range[1] + enemy.max_move)
                {
                    benefit += 2 * enemy.atk;
                }
            }

            else if (type == "protect_my_barrack")
            {
                int dis = cube_distance(enemy.pos, my_barrack);
                if (enemy.max_move <= dis)
                {
                    if (getUnitByPos(my_barrack, false).id == -1)
                        return enemy.id;
                    else
                        benefit += 2 * enemy.atk;
                }
            }
            else if (type == "protect_enemy_barrack")
            {
                int dis = cube_distance(enemy.pos, enemy_barrack);
                if (enemy.max_move <= dis)
                {
                    if (getUnitByPos(enemy_barrack, false).id == -1)
                        return enemy.id;
                    else
                        benefit += 2 * enemy.atk;
                }
            }

            if (ally.atk >= enemy.hp)
            {
                benefit += enemy_extra.priority * enemy.level;
            }

            else
            {
                benefit += ally.atk * enemy.level;
            }

            if (AiClient::canAttack(enemy, ally))
            {
                health -= enemy.atk;
                if (enemy.atk >= ally.hp)
                {
                    benefit -= ally.hp * ally.level;
                }
                else
                {
                    benefit -= enemy.atk * ally.level;
                }
            }

            if (health > 0)
            {
                //考虑下回合
                for (const auto &enemy_next_round : enemy_list)
                {
                    struct unit_info &enemy_next_round_extra = unit_extra_info[enemy_next_round.id];

                    if (AiClient::canAttack(enemy_next_round, ally))
                    {
                        if (enemy_next_round.atk >= health)
                        {
                            benefit -= health * ally.level;
                        }
                        else
                        {
                            benefit -= enemy_next_round.atk * ally.level;
                        }

                        if (AiClient::canAttack(ally, enemy_next_round))
                        {
                            if (ally.atk >= enemy_next_round.hp)
                            {
                                benefit += enemy_next_round_extra.priority * enemy_next_round.level;
                            }

                            else
                            {
                                benefit += ally.atk * enemy_next_round.level;
                            }
                        }
                        health -= enemy_next_round.atk;
                        if (health <= 0)
                            break;
                    }
                }
            }

            target_id_counter[enemy.id] = benefit;
        }
    }

    for (auto target : target_list)
    {
        if (target_id_counter[target.id] > max_benefit)
        {
            best_target = target.id;
            max_benefit = target_id_counter[target.id];
        }
    }

    //考虑神迹

    int dis = cube_distance(ally.pos, enemy_miracle_pos);
    if (ally.atk_range[0] <= dis && dis <= ally.atk_range[1])
    {
        int benefit = 0;
        benefit = ally.atk * 5;
        //考虑下回合
        int health = ally.hp;
        for (const auto &enemy_next_round : enemy_list)
        {
            struct unit_info &enemy_next_round_extra = unit_extra_info[enemy_next_round.id];
            if (AiClient::canAttack(enemy_next_round, ally))
            {
                if (enemy_next_round.atk >= health)
                {
                    benefit -= health * ally.level;
                }
                else
                {
                    benefit -= enemy_next_round.atk * ally.level;
                }

                if (AiClient::canAttack(ally, enemy_next_round))
                {
                    if (ally.atk >= enemy_next_round.hp)
                    {
                        benefit += enemy_next_round_extra.priority * enemy_next_round.level;
                    }

                    else
                    {
                        benefit += ally.atk * enemy_next_round.level;
                    }
                }
                health -= enemy_next_round.atk;
                if (health <= 0)
                    break;
            }
        }
        if (benefit >= max_benefit)
        {
            best_target = my_camp ^ 1;
            max_benefit = benefit;
        }
    }

    return best_target;
}

Pos AI::counter(vector<Pos> &pos_list, string type)
{
    std::map<Pos, int> map_counter;
    if (type == "inferno_flame")
    {
        for (auto pos : pos_list)
        {
            int score = 0;
            auto unit_list = units_in_range(pos, 2, map, my_camp ^ 1);
            for (auto unit : unit_list)
            {
                if (unit.hp <= 2)
                    score += unit.level * ENEMY_DEATH;
                else
                    score += 2 * ENEMY_HURT;
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

/*
//我方单位目标 des=1,占领destination;des=2,直线进攻基地;des=3，保护神迹；des=4,保护我方驻扎点；des=5，保护对方驻扎点
                if (ally_extra.des == 1)
                {
                    decide_pos = counter(reach_pos_list, "close", ally);
                }

                else if (ally_extra.des == 2)
                {
                    decide_pos = counter(reach_pos_list, "atk_miracle", ally);
                }

                else if (ally_extra.des == 3)
                {
                    decide_pos = counter(reach_pos_list, "protect_miracle", ally);
                }

                else if (ally_extra.des == 4)
                {
                    decide_pos = counter(reach_pos_list, "protect_my_barrack", ally);
                }

                else if (ally_extra.des == 5)
                {
                    decide_pos = counter(reach_pos_list, "protect_enemy_barrack", ally);
*/

Pos AI::counter(vector<Pos> &pos_list, string type, Unit &my_unit)
{
    std::map<Pos, int> map_counter;
    struct unit_info &my_extra = unit_extra_info[my_unit.id];

    for (auto pos : pos_list)
    {
        int benefit = 0;
        auto unit_list = units_in_range(pos, 4, map, my_camp ^ 1);

        //特殊情况
        if (type == "close" && pos==my_extra.destination)
        {
            return pos;
        }

        //常规计算，能打到和不被打到
        for (auto unit : unit_list)
        {
            struct unit_info &extra = unit_extra_info[unit.id];

            if (AiClient::canAttack(my_unit, unit)) //我能打到敌人
            {
                if (my_unit.atk >= unit.hp)
                {
                    benefit += extra.priority * unit.level;
                }

                else
                {
                    benefit += my_unit.atk * unit.level;
                }
            }

            if (AiClient::canAttack(unit, my_unit))
            {
                if (unit.atk >= my_unit.hp)
                {
                    benefit -= my_unit.hp * my_unit.level;
                }
                else
                {
                    benefit -= unit.atk * my_unit.level;
                }
                if (extra.type == "avoid")
                    benefit -= 100;
            }
        }
        if (type == "close")
        {
            benefit += (20 - cube_distance(pos, my_extra.destination)) * 10;
        }
        else if (type == "atk_miracle")
        {
            benefit += (20 - cube_distance(pos, enemy_miracle_pos)) * 20;
        }
        else if (type == "protect_miracle")
        {
            benefit += (20 - cube_distance(pos, miracle_pos)) * 10;
        }
        else if (type == "protect_my_barrack")
        {
            benefit += (20 - cube_distance(pos, my_barrack)) * 10;
        }
        else if (type == "protect_enemy_barrack")
        {
            benefit += (20 - cube_distance(pos, enemy_barrack)) * 10;
        }
        else
        {
            benefit += (20 - cube_distance(pos, enemy_miracle_pos)) * 5;
        }
        map_counter[pos] = benefit;
    }

    vector<Pos> &enemy_summon = getSummonPosByCamp(my_camp ^ 1);

    for (auto pos : enemy_summon) //占用对方出兵点
    {
        if (map_counter.find(pos) != map_counter.end())
            map_counter[pos] += 10;
    }

    auto best_pos = miracle_pos;
    int max_benefit = 0;
    if (type == "attack")
        max_benefit = -9999;
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

Pos AI::posShift(Pos pos, string direct, const int &lenth = 1)
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
        Pos decide_pos = counter(postions, "inferno_flame");
        use(players[my_camp].artifact[0].id, decide_pos);
        sign_unit(2);
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
