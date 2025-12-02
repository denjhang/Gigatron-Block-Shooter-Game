# blit_test166.c 功能文档

## 概述

blit_test166.c 是一个为 Gigatron 平台开发的射击游戏，具有复杂的敌人AI、智能演示模式和自适应难度系统。游戏玩家控制一个方块，在网格背景上与各种敌人战斗，支持四方向移动和四方向射击。

## 主要功能

### 1. 基础游戏机制

- **玩家控制**：支持上下左右四方向移动，初始速度为3像素/帧
- **射击系统**：
  - A键：左右同时发射子弹
  - B键：上下同时发射子弹
  - 子弹速度：10像素/帧
  - 最大同时存在4颗子弹
  - 射击间隔：3帧

### 2. 敌人系统

- **敌人类型**：根据颜色区分，不同颜色有不同血量
  - 黄色(0x3E)：3点血量
  - 橙色(0x3C)：2点血量
  - 绿色(0x3A)、青色(0x38)、蓝色(0x36)：1点血量

- **敌人行为模式**：
  - 直线移动：固定方向移动
  - 随机曲线移动：定期改变方向
  - 追击玩家：计算朝向玩家的方向移动

- **敌人生成**：
  - 从屏幕边缘生成，避开四个角落的20x20区域
  - 初始向屏幕内部移动
  - 移动至少20像素后才能离开屏幕

### 3. 智能演示模式

- **AI学习系统**：
  - 8种行为类型（静止、四方向移动、四方向斜线移动）
  - 行为评分系统，根据击中率、移动效率等评分
  - 自适应行为选择，优先使用高分行为

- **智能瞄准**：
  - 预判敌人未来位置（5-8帧）
  - 计算子弹飞行时间，调整瞄准点
  - 优先攻击血量少、距离近的敌人

- **智能躲避**：
  - 检测被包围状态（周围40像素内有3个或更多敌人）
  - 被包围时计算合力方向，向相反方向突围
  - 智能选择突围方向，选择敌人最少的方向

## AI评分学习系统详解

### 1. 系统架构

AI评分学习系统是演示模式的核心，它通过不断尝试和评估不同行为的效果，逐步优化AI的游戏策略。系统主要由以下几个部分组成：

- **行为类型定义**：8种不同的移动和射击行为
- **评分机制**：根据游戏结果为每种行为打分
- **行为选择算法**：基于评分选择最优行为
- **学习反馈**：根据射击结果更新评分

### 2. 行为类型详解

系统定义了8种基本行为类型，每种行为都有特定的移动模式和射击策略：

| 行为ID | 行为名称 | 移动方向 | 斜线移动 | 移动时射击 | 描述 |
|--------|----------|----------|----------|------------|------|
| 0 | 静止 | 无 | 否 | 否 | 原地不动，不移动时不能射击 |
| 1 | 向右移动 | 右 | 否 | 是 | 水平向右移动 |
| 2 | 向左移动 | 左 | 否 | 是 | 水平向左移动 |
| 3 | 向下移动 | 下 | 否 | 是 | 垂直向下移动 |
| 4 | 向上移动 | 上 | 否 | 是 | 垂直向上移动 |
| 5 | 右上斜线移动 | 右上 | 是 | 是 | 向右上方斜线移动 |
| 6 | 右下斜线移动 | 右下 | 是 | 是 | 向右下方斜线移动 |
| 7 | 左上斜线移动 | 左上 | 是 | 是 | 向左上方斜线移动 |
| 默认 | 左下斜线移动 | 左下 | 是 | 是 | 向左下方斜线移动 |

### 3. 评分机制

评分机制是AI学习的核心，它根据游戏中的各种事件为不同行为打分：

#### 3.1 评分参数

```c
static int hit_reward = 15;              // 击中敌人的奖励分数
static int miss_penalty = 3;              // 未击中敌人的惩罚分数
static int random_shoot_bonus = 5;        // 随机射击的额外奖励分数
static int shoot_frequency_bonus = 3;      // 高频射击的额外奖励分数
static int aim_offset_bonus = 10;         // 瞄准偏移量的奖励分数
static int good_aim_bonus = 8;           // 良好瞄准的奖励分数
```

#### 3.2 评分触发条件

1. **击中敌人**：
   - 基础奖励：+15分
   - 随机射击额外奖励：33%概率+5分
   - 行为尝试次数+1

2. **未击中敌人**：
   - 基础惩罚：-3分
   - 行为尝试次数+1

3. **射击频率**：
   - 每次射击：+3分
   - 长时间不射击（1秒）：-2分

4. **瞄准质量**：
   - 高质量瞄准（偏移量<5像素）：+8分
   - 中等质量瞄准（偏移量<10像素）：+10分

### 4. 行为选择算法

行为选择算法综合考虑了评分、探索和随机性：

#### 4.1 评分计算

```c
// 计算平均得分（总得分/尝试次数）
int avg_score = 0;
if (behavior_attempts[i] > 0) {
    avg_score = behavior_scores[i] / behavior_attempts[i];
}
```

#### 4.2 探索奖励

为了鼓励尝试新行为，系统为尝试次数少的行为提供额外奖励：

```c
// 大幅增加探索机会，鼓励随机行为
if (behavior_attempts[i] < 5) {
    avg_score += 10; // 增加探索奖励
}
```

#### 4.3 随机性

为了增加AI的不可预测性和适应性，系统引入了随机因素：

```c
// 随机给予额外奖励，增加随机性
if (SYS_Random() % 4 == 0) {
    avg_score += 8; // 25%概率获得额外奖励
}

// 20%概率完全随机选择行为，增加随机性
if (SYS_Random() % 5 == 0) {
    best_behavior = SYS_Random() % MAX_BEHAVIOR_TYPES;
}
```

#### 4.4 移动行为奖励

系统特别鼓励移动和射击结合的行为：

```c
// 特别奖励移动和射击结合的行为
if (i >= 1 && i <= 7) { // 所有移动行为
    avg_score += 3; // 移动行为额外奖励
}
```

### 5. 学习反馈机制

学习反馈机制根据游戏结果实时更新评分：

#### 5.1 击中反馈

当子弹击中敌人时，系统会：

1. 增加当前行为的得分
2. 重置连续未击中计数
3. 记录射击结果为"击中"
4. 给予随机射击额外奖励（33%概率）

#### 5.2 未击中反馈

当射击未击中时，系统会：

1. 减少当前行为的得分
2. 增加连续未击中计数
3. 记录射击结果为"未击中"

#### 5.3 连续未击中处理

当连续未击中次数过多时，AI会调整策略：

```c
// AI学习：根据连续未击中次数决定是否需要移动后再射击
int should_move_before_shoot = (consecutive_misses >= 2);
```

### 6. 高级AI特性

#### 6.1 敌人预判

AI不仅瞄准敌人的当前位置，还会预判敌人未来位置：

```c
// 预判敌人未来位置（基于敌人移动方向和速度）
int predict_frames = 5; // 预判5帧后的位置
int predict_x = enemies[i].sprite.screen_x + enemies[i].dx * predict_frames;
int predict_y = enemies[i].sprite.screen_y + enemies[i].dy * predict_frames;
```

#### 6.2 子弹飞行时间计算

AI会计算子弹到达目标位置需要的时间，并调整瞄准点：

```c
// 计算子弹到达目标位置需要的时间
int bullet_speed = 10; // 子弹速度为10像素/帧
int travel_time_x = abs(dx) / bullet_speed;
int travel_time_y = abs(dy) / bullet_speed;
int max_travel_time = (travel_time_x > travel_time_y) ? travel_time_x : travel_time_y;

// 考虑敌人移动，调整瞄准点
int adjusted_target_x = target_x + target->dx * max_travel_time;
int adjusted_target_y = target_y + target->dy * max_travel_time;
```

#### 6.3 威胁评估

AI会评估每个敌人的威胁程度，优先处理高威胁目标：

```c
// 计算威胁评分
int threat_score = 0;

// 距离越近威胁越大
threat_score += (100 - distance);

// 敌人速度越快威胁越大
int enemy_speed = abs(enemies[i].dx) + abs(enemies[i].dy);
threat_score += enemy_speed * 5;

// 追击型敌人威胁更大
if (enemies[i].move_type == 2) {
    threat_score += 20;
}

// 血量多的敌人威胁更大
threat_score += enemies[i].health * 5;

// 如果敌人正在接近玩家，威胁更大
int approach_vector = (enemy_center_x - player_center_x) * enemies[i].dx +
                    (enemy_center_y - player_center_y) * enemies[i].dy;
if (approach_vector < 0) { // 点积为负表示正在接近
    threat_score += 15;
}
```

### 7. 学习效果

通过AI评分学习系统，演示模式下的AI能够：

1. **逐步优化行为选择**：优先使用得分高的行为
2. **适应不同游戏情况**：根据敌人类型和数量调整策略
3. **提高射击精度**：通过预判和瞄准质量评分提高命中率
4. **平衡探索与利用**：在尝试新行为和使用已知好行为之间保持平衡
5. **动态调整策略**：根据连续未击中次数调整射击策略

这个AI评分学习系统使演示模式不仅仅是简单的预设行为序列，而是一个能够学习和适应的智能系统，大大提高了游戏的观赏性和技术展示价值。

### 4. 难度系统

- **动态难度调整**：
  - 每击杀5个敌人，难度增加1（最高5级）
  - 难度提升增加最大活动敌人数量（最多7个）
  - 敌人最低速度随玩家被袭击次数增加

- **滚动系统**：
  - 所有敌人被击杀后触发自动滚动
  - 滚动期间禁止射击
  - 滚动距离影响敌人生成频率和数量

### 5. 生命值和奖励系统

- **玩家生命值**：初始3条生命
- **生命值损失**：与敌人碰撞损失1条生命
- **奖励机制**：
  - 每击杀5个敌人增加1条生命并恢复颜色
  - 每击杀2个敌人移动速度增加1（最高7）
  - 玩家颜色随生命值变化：白色→浅蓝→红色

### 6. 背景和视觉效果

- **网格背景**：10x10像素网格，蓝色背景配绿色水平线和青色垂直线
- **精灵系统**：使用6x6像素精灵，支持多精灵拼接
- **背景恢复**：智能恢复精灵移动后的背景，支持精细恢复和简单恢复两种模式

## 数据结构

### 1. Sprite 结构体

```c
typedef struct {
    int screen_x, screen_y; // 精灵在屏幕上的相对位置（不受滚动影响）
    int x, y;             // 精灵在显存中的真实位置（受滚动影响）
    int w, h;             // 精灵的宽度和高度
    int color;            // 精灵的颜色
    int init_x, init_y;   // 精灵的初始位置
    int width_units;       // 宽度单位数（1-4，每个单位6像素）
    int height_units;      // 高度单位数（1-4，每个单位6像素）
} Sprite;
```

### 2. Bullet 结构体

```c
typedef struct {
    Sprite sprite;   // 子弹精灵
    int dx, dy;     // 子弹移动方向和速度
    int active;      // 子弹是否活跃
} Bullet;
```

### 3. Enemy 结构体

```c
typedef struct {
    Sprite sprite;           // 敌人精灵
    int world_x, world_y;   // 敌人在游戏世界中的绝对坐标
    int dx, dy;             // 敌人移动方向和速度
    int active;             // 敌人是否活跃
    int health;             // 敌人血量
    int move_type;          // 移动类型：0=直线，1=随机曲线，2=追击玩家
    int speed;              // 移动速度(1-3)
    int curve_counter;      // 用于随机曲线移动的计数器
    int distance_traveled;  // 敌人移动的距离
    int original_color;     // 敌人原始颜色
    int hit_color_index;    // 被击中后的颜色阶段索引
    int state;              // 敌人状态：0=进入中，1=活跃
    int stationary_timer;    // 静止计时器，记录敌人静止的时间（帧数）
    int prev_x, prev_y;     // 上一帧的位置，用于检测是否静止
} Enemy;
```

## 核心算法

### 1. 坐标转换算法

```c
static void screen_to_real_coords(Sprite *sprite)
{
    // 真实X坐标 = 屏幕X坐标 + 滚动偏移量
    sprite->x = sprite->screen_x + (scrollX % GRID_SIZE);
    
    // 如果真实X坐标超出显存范围，进行循环处理
    if (sprite->x < 0) sprite->x += MAX_SCROLL + 1;
    if (sprite->x > MAX_SCROLL) sprite->x -= MAX_SCROLL + 1;
    
    // Y坐标不受水平滚动影响
    sprite->y = sprite->screen_y;
}
```

### 2. 背景恢复算法

程序实现了两种背景恢复算法：

1. **精细恢复**：计算新旧精灵位置的交集，只恢复非重叠部分
2. **简单恢复**：完全恢复旧精灵区域，适用于精灵跨越显存边界的情况

### 3. AI行为选择算法

```c
static int select_best_behavior()
{
    int best_behavior = 0;
    int best_score = -9999;
    int i;
    
    // 遍历所有行为类型，选择得分最高的
    for (i = 0; i < MAX_BEHAVIOR_TYPES; i++) {
        // 计算平均得分（总得分/尝试次数）
        int avg_score = 0;
        if (behavior_attempts[i] > 0) {
            avg_score = behavior_scores[i] / behavior_attempts[i];
        }
        
        // 大幅增加探索机会，鼓励随机行为
        if (behavior_attempts[i] < 5) {
            avg_score += 10; // 增加探索奖励
        }
        
        // 随机给予额外奖励，增加随机性
        if (SYS_Random() % 4 == 0) {
            avg_score += 8; // 25%概率获得额外奖励
        }
        
        // 特别奖励移动和射击结合的行为
        if (i >= 1 && i <= 7) { // 所有移动行为
            avg_score += 3; // 移动行为额外奖励
        }
        
        if (avg_score > best_score) {
            best_score = avg_score;
            best_behavior = i;
        }
    }
    
    // 20%概率完全随机选择行为，增加随机性
    if (SYS_Random() % 5 == 0) {
        best_behavior = SYS_Random() % MAX_BEHAVIOR_TYPES;
    }
    
    return best_behavior;
}
```

### 4. 敌人预判算法

```c
// 预判敌人未来位置（基于敌人移动方向和速度）
int predict_frames = 5; // 预判5帧后的位置
int predict_x = enemies[i].sprite.screen_x + enemies[i].dx * predict_frames;
int predict_y = enemies[i].sprite.screen_y + enemies[i].dy * predict_frames;

// 计算子弹到达目标位置需要的时间
int bullet_speed = 10; // 子弹速度为10像素/帧
int travel_time_x = abs(dx) / bullet_speed;
int travel_time_y = abs(dy) / bullet_speed;
int max_travel_time = (travel_time_x > travel_time_y) ? travel_time_x : travel_time_y;

// 考虑敌人移动，调整瞄准点
int adjusted_target_x = target_x + target->dx * max_travel_time;
int adjusted_target_y = target_y + target->dy * max_travel_time;
```

## 控制说明

### 正常模式控制

- **方向键**：上下左右移动
- **A键**：左右发射子弹
- **B键**：上下发射子弹（滚动时禁用）
- **Start键**：切换玩家颜色/切换演示模式
- **Select键**：无功能

### 演示模式

- 按Start键可切换到演示模式
- 演示模式下AI自动控制玩家移动和射击
- AI会学习并优化自己的行为策略

## 技术特点

1. **高效渲染**：使用Gigatron的精灵系统和硬件滚动
2. **智能背景恢复**：根据情况选择精细或简单恢复算法
3. **自适应AI**：演示模式下的AI会根据游戏情况学习并调整策略
4. **动态难度**：根据玩家表现调整游戏难度
5. **流畅动画**：60FPS的游戏循环，确保流畅的游戏体验

## 编译和运行

使用以下命令编译：
```
glcc -o blit_test166.gt1 blit_test166.c -rom=dev7 -map=64k,./blit_test141.ovl --no-runtime-bss
```

编译成功后生成blit_test166.gt1文件，可在Gigatron模拟器或硬件上运行。