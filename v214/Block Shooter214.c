#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <gigatron/libc.h>
#include <gigatron/sys.h>
#include <gigatron/console.h>
#include <gigatron/kbget.h>
#include <gigatron/sound.h>

#define WIDTH 160
#define HEIGHT 120
#define GRID_SIZE 10  // 网格大小，与背景绘制保持一致
#define CORNER_SIZE 20 // 角落区域大小
#define MAX_SCROLL 170  // 最大滚动值，设置为250

// 敌人状态定义
#define ENEMY_STATE_ENTERING 0 // 敌人正在进入游戏区域
#define ENEMY_STATE_ACTIVE   1 // 敌人已进入游戏区域并活跃

/* 按键定义 */
#define KEY_LEFT  buttonLeft
#define KEY_RIGHT buttonRight
#define KEY_UP    buttonUp
#define KEY_DOWN  buttonDown
#define KEY_A     buttonA
#define KEY_B     buttonB
#define KEY_START buttonStart
#define KEY_SELECT buttonSelect

typedef struct {
    int screen_x, screen_y; // 精灵在屏幕上的相对位置（不受滚动影响）
    int x, y;       // 精灵在显存中的真实位置（受滚动影响）
    int w, h;       // 精灵的宽度和高度
    int color;      // 精灵的颜色
    int init_x, init_y; // 精灵的初始位置
    int width_units;  // 宽度单位数（1-4，每个单位6像素）
    int height_units; // 高度单位数（1-4，每个单位6像素）
} Sprite;

/* 子弹结构体 */
typedef struct {
    Sprite sprite;          // 子弹精灵
    int dx, dy;            // 子弹移动方向和速度
    int active;            // 子弹是否活跃
} Bullet;

#define MAX_BULLETS 4     // 最大子弹数量
static Bullet bullets[MAX_BULLETS];  // 子弹数组
static int active_bullet_count = 0; // 活跃子弹计数

/* 敌人结构体 */
typedef struct {
    Sprite sprite;          // 敌人精灵
    int world_x, world_y;  // 敌人在游戏世界中的绝对坐标
    int dx, dy;            // 敌人移动方向和速度
    int active;            // 敌人是否活跃
    int health;            // 敌人血量
    int move_type;         // 移动类型：0=直线，1=随机曲线，2=追击玩家
    int speed;             // 移动速度(1-3)
    int curve_counter;     // 用于随机曲线移动的计数器
    int distance_traveled;  // 敌人移动的距离
    int original_color;    // 敌人原始颜色
    int hit_color_index;   // 被击中后的颜色阶段索引
    int state;             // 敌人状态：0=进入中，1=活跃
    int stationary_timer;   // 静止计时器，记录敌人静止的时间（帧数）
    int prev_x, prev_y;    // 上一帧的位置，用于检测是否静止
} Enemy;

#define MAX_ENEMIES 7       // 最大敌人数量
static Enemy enemies[MAX_ENEMIES];  // 敌人数组
static int active_enemy_count = 0;   // 活跃敌人计数
static int enemy_spawn_timer = 0;    // 敌人生成计时器
static int enemy_hit_count = 0;      // 敌人成功袭击玩家的次数

// 新增全局变量
static int current_difficulty = 1;     // 当前难度指数，初始为1
static int enemies_killed_for_difficulty = 0; // 用于难度提升的击杀计数
static int max_active_enemies = 3;     // 当前允许的最大活动敌人数量，初始为3
static int player_total_kills = 0;     // 玩家总击杀数，用于奖励

// 敌人被击中后的颜色序列，从白色开始，然后逐渐变暗
static int enemy_hit_colors[] = {0x3F, 0x3E, 0x3C, 0x3A, 0x38, 0x36, 0x34, 0x32, 0x30}; // 白色，黄色，橙色，绿色，青色，蓝色，紫色，红色，黑色
#define MAX_ENEMY_HIT_COLORS (sizeof(enemy_hit_colors) / sizeof(enemy_hit_colors[0]))

/* 玩家生命值系统 */
static int player_lives = 3;         // 玩家生命值
static int player_colors[] = {0x3F, 0x0F, 0x03}; // 玩家颜色：白色、浅蓝、红色
static int current_player_color_index = 0;       // 当前玩家颜色索引

/* 精灵位置变量 */
static Sprite playerSprite;
static Sprite prevPlayerSprite;
static int player_speed = 3; // 主角移动速度，初始为3
static int player_kill_count = 0; // 玩家击杀敌人计数

static byte frame_counter = 0; // 添加帧计数器
static int scrollX = 0; // 水平滚动偏移量
static int scrollDirection = 0; // 滚动方向：-1向左，0不滚动，1向右
static byte *ShiftControl = (byte*)0x0101; // ShiftControl 寄存器地址

// 新增滚动相关变量
static int total_scrolled_distance = 0; // 总滚动距离
static int enemy_spawn_frequency_divisor = 1; // 敌人生成频率除数，初始为1
static int enemy_spawn_max_count_increase = 0; // 敌人生成最大数量增加值，初始为0

// 新增自动滚动相关变量
static int auto_scroll_active = 0; // 自动滚动是否激活
static int auto_scroll_target_distance = 0; // 自动滚动的目标距离
static int auto_scroll_current_distance = 0; // 当前自动滚动的距离
static int auto_scroll_direction = 0; // 自动滚动的方向
static int auto_scroll_preparing = 0; // 自动滚动准备状态，0=未准备，1=准备中
static int shooting_disabled = 0; // 射击是否被禁用，0=启用，1=禁用

// 演示模式相关变量
static int demo_mode = 0; // 演示模式标志，0=关闭，1=开启
static int demo_move_timer = 0; // 演示模式移动计时器
static int demo_shoot_timer = 0; // 演示模式射击计时器
static int demo_direction = 0; // 演示模式移动方向，0=上，1=右，2=下，3=左
static int demo_target_enemy = -1; // 演示模式目标敌人索引
static int demo_surrounded = 0; // 演示模式被包围状态，0=未被包围，1=被包围
static int demo_surrounded_timer = 0; // 演示模式被包围状态计时器
static int demo_aim_timer = 0; // 演示模式瞄准计时器
static int demo_stable_direction_timer = 0; // 演示模式稳定方向计时器
static int demo_last_direction = -1; // 演示模式上一次的方向
static int demo_aimed_shot = 0; // 演示模式是否已瞄准
static int demo_enemy_predict_x = 0; // 演示模式敌人预测X坐标
static int demo_enemy_predict_y = 0; // 演示模式敌人预测Y坐标
static int demo_rapid_fire_timer = 0; // 演示模式快速连发计时器
static int demo_horizontal_target = -1; // 演示模式水平移动目标敌人索引

// 新增斜线移动相关变量
static int demo_diagonal_move = 0; // 演示模式是否使用斜线移动，0=否，1=是
static int demo_dx = 0; // 演示模式X方向速度分量
static int demo_dy = 0; // 演示模式Y方向速度分量
static int demo_can_shoot_while_moving = 1; // 演示模式是否可以在移动时射击，0=否，1=是

// AI行为打分系统相关变量
#define MAX_BEHAVIOR_TYPES 8 // 最大行为类型数
static int behavior_scores[MAX_BEHAVIOR_TYPES]; // 各种行为的得分
static int behavior_attempts[MAX_BEHAVIOR_TYPES]; // 各种行为的尝试次数
static int last_behavior_type = -1; // 上一次使用的行为类型
static int last_shoot_result = 0; // 上一次射击结果，0=未射击，1=击中，-1=未击中
static int consecutive_misses = 0; // 连续未击中次数
static int ai_learning_enabled = 1; // AI学习是否启用，0=禁用，1=启用
static int shoot_without_moving_penalty = 2; // 不移动就射击的惩罚分数
static int hit_reward = 15; // 击中敌人的奖励分数，增加奖励
static int miss_penalty = 3; // 未击中敌人的惩罚分数，恢复扣分
static int random_shoot_bonus = 5; // 随机射击的额外奖励分数
static int shoot_frequency_bonus = 3; // 高频射击的额外奖励分数
static int aim_offset_bonus = 10; // 瞄准偏移量的奖励分数
static int good_aim_bonus = 8; // 良好瞄准的奖励分数

// 主角子弹发射计时器
static int player_shoot_timer = 0; // 主角子弹发射计时器
static int player_shoot_interval = 3; // 主角子弹发射间隔，3帧

// 函数声明
static int select_best_behavior();
static void apply_behavior(int behavior_type);

void bg()
{
	int i, j;
	clrscr();
	
	// 填充整个显存区域（x=0到250），确保所有可能的滚动位置都有背景
	for (j = 0; j <= MAX_SCROLL; j += GRID_SIZE) {
		// 填充蓝色背景
		SYS_Fill(makew(8, j), 0x10, makew(120, GRID_SIZE));
		
		// 绘制水平网格线
		for (i = 0; i < 120; i += GRID_SIZE) {
			SYS_Fill(makew(8+i, j), 0x20, makew(1, GRID_SIZE));
		}
	}
	
	// 绘制垂直线（覆盖整个显存区域）
	for (i = 0; i <= MAX_SCROLL; i += GRID_SIZE) {
		SYS_Fill(makew(8, i), 0x30, makew(120, 1));
	}
	
	// 设置ShiftControl寄存器初始值
	*ShiftControl = 0;
}


/* 初始化精灵 */
static void init_sprite(Sprite *sprite, int x, int y, int w, int h, int color)
{
	sprite->screen_x = x;  // 屏幕相对位置
	sprite->screen_y = y;  // 屏幕相对位置
	sprite->x = x;         // 真实位置（初始时与屏幕位置相同）
	sprite->y = y;         // 真实位置（初始时与屏幕位置相同）
	sprite->init_x = x;
	sprite->init_y = y;
	sprite->w = w;
	sprite->h = h;
	sprite->color = color;
	sprite->width_units = 1;  // 默认宽度单位数为1
	sprite->height_units = 1; // 默认高度单位数为1
}

/* 将屏幕相对坐标转换为显存真实坐标 */
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

/* 恢复精灵位置的背景 - 使用blit恢复变化的区域，只恢复更改部分，使用真实坐标 */
static void restore_sprite_background(Sprite *prev, Sprite *current, int use_simple_restore)
{
    // 如果指定使用简单恢复，或者精灵跨越显存边界，则使用简单恢复方法
    int prev_right = prev->x + prev->w;
    int current_right = current->x + current->w;
    
    if (use_simple_restore || prev_right > MAX_SCROLL || current_right > MAX_SCROLL) {
        // 恢复完整的旧精灵区域
        if (prev_right <= MAX_SCROLL) {
            // 旧精灵完全在显存范围内
            SYS_Blit(makew(8+prev->y, prev->x), makew(8+(prev->y%10), prev->x%10), makew(prev->h, prev->w));
        } else {
            // 旧精灵跨越边界，分两部分恢复
            int left_width = MAX_SCROLL - prev->x;
            int right_width = prev_right - MAX_SCROLL;
            
            // 恢复左半部分
            SYS_Blit(makew(8+prev->y, prev->x), makew(8+(prev->y%10), prev->x%10), makew(prev->h, left_width));
            // 恢复右半部分（从0开始）
            SYS_Blit(makew(8+prev->y, 0), makew(8+(prev->y%10), 0), makew(prev->h, right_width));
        }
        return;
    }
    
    // 如果精灵位置和尺寸都没有改变，则不需要恢复
    if (prev->x == current->x && prev->y == current->y &&
        prev->w == current->w && prev->h == current->h) {
        return;
    }

    // 旧矩形 (prev->x, prev->y, prev->w, prev->h)
    // 新矩形 (current->x, current->y, current->w, current->h)

    // 计算交集
    int ix = (prev->x > current->x) ? prev->x : current->x;
    int iy = (prev->y > current->y) ? prev->y : current->y;
    int ir = ((prev->x + prev->w) < (current->x + current->w)) ? (prev->x + prev->w) : (current->x + current->w);
    int ib = ((prev->y + prev->h) < (current->y + current->h)) ? (prev->y + prev->h) : (current->y + current->h);

    int iw = (ir > ix) ? (ir - ix) : 0;
    int ih = (ib > iy) ? (ib - iy) : 0;

    // 恢复旧矩形中不与新矩形重叠的部分
    // 顶部条带
    if (iy > prev->y) {
        SYS_Blit(makew(8+prev->y, prev->x), makew(8+(prev->y%10), prev->x%10), makew(iy - prev->y, prev->w));
    }
    // 底部条带
    if (ib < prev->y + prev->h) {
        SYS_Blit(makew(8+ib, prev->x), makew(8+(ib%10), prev->x%10), makew((prev->y + prev->h) - ib, prev->w));
    }
    // 左侧条带 (在交集高度范围内)
    if (ix > prev->x) {
        SYS_Blit(makew(8+iy, prev->x), makew(8+(iy%10), prev->x%10), makew(ih, ix - prev->x));
    }
    // 右侧条带 (在交集高度范围内)
    if (ir < prev->x + prev->w) {
        SYS_Blit(makew(8+iy, ir), makew(8+(iy%10), ir%10), makew(ih, (prev->x + prev->w) - ir));
    }
}

/* 绘制精灵 - 使用精灵指令，支持多个精灵拼接，使用真实坐标 */
static void draw_sprite(Sprite *sprite)
{
	// 创建6x6精灵数据，37个元素，最后一个为十进制的250
	static char sprite_data[37];
	int i;
	for (i = 0; i < 36; i++) {
		sprite_data[i] = sprite->color;
	}
	sprite_data[36] = 250; // 最后一个元素设置为十进制的250
	
	// 根据精灵的宽度和高度单位数绘制精灵矩阵
	int row, col;
	for (row = 0; row < sprite->height_units; row++) {
		for (col = 0; col < sprite->width_units; col++) {
			// 计算每个精灵的位置，使用真实坐标
			unsigned int addr = makew(8+sprite->y + row*6, sprite->x + col*6);
			SYS_Sprite6(sprite_data, (void*)addr);
		}
	}
}

/* 初始化子弹系统 */
static void init_bullets()
{
    int i;
    for (i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = 0;
        bullets[i].sprite.w = 6;  // 子弹大小与方块相同
        bullets[i].sprite.h = 6;
        bullets[i].sprite.width_units = 1;
        bullets[i].sprite.height_units = 1;
        bullets[i].sprite.color = 0x3F;  // 默认颜色，发射时会更新为方块颜色
    }
    active_bullet_count = 0; // 初始化活跃子弹计数
}

/* 初始化敌人系统 */
static void init_enemies()
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
        enemies[i].sprite.w = 6;  // 敌人大小与方块相同
        enemies[i].sprite.h = 6;
        enemies[i].sprite.width_units = 1;
        enemies[i].sprite.height_units = 1;
        enemies[i].stationary_timer = 0; // 初始化静止计时器
        enemies[i].prev_x = 0; // 初始化上一帧位置
        enemies[i].prev_y = 0;
    }
    active_enemy_count = 0; // 初始化活跃敌人计数
    enemy_spawn_timer = 0;  // 初始化敌人生成计时器
}

/* 生成新敌人 */
static void spawn_enemy()
{
    int i;
    
    // 检查是否已达到最大敌人数量
    if (active_enemy_count >= max_active_enemies) {
        return;
    }
    
    // 找到一个未使用的敌人槽位
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            // 定义所有可能的敌人颜色
            int available_colors[] = {0x3E, 0x3C, 0x3A, 0x38, 0x36}; // 黄色、橙色、绿色、青色、蓝色
            int color_count = sizeof(available_colors) / sizeof(available_colors[0]);
            int selected_color;
            
            // 随机选择一个颜色，确保不与主角颜色相同
            int valid_color_found = 0;
            int attempts = 0;
            int color_idx; // 将声明移到循环外部
            while (!valid_color_found && attempts < 100) { // 增加尝试次数以确保找到不同颜色
                selected_color = available_colors[SYS_Random() % color_count];
                if (selected_color != playerSprite.color) {
                    enemies[i].sprite.color = selected_color;
                    valid_color_found = 1;
                }
                attempts++;
            }
            // 如果100次尝试都失败，使用第一个不等于主角颜色的颜色
            if (!valid_color_found) {
                for (color_idx = 0; color_idx < color_count; color_idx++) {
                    if (available_colors[color_idx] != playerSprite.color) {
                        enemies[i].sprite.color = available_colors[color_idx];
                        break;
                    }
                }
            }
            
            // 根据颜色设置血量
            switch (enemies[i].sprite.color) {
                case 0x3E: // 黄色
                    enemies[i].health = 3;
                    break;
                case 0x3C: // 橙色
                    enemies[i].health = 2;
                    break;
                default: // 绿色、青色、蓝色
                    enemies[i].health = 1;
                    break;
            }
            
            // 保存原始颜色
            enemies[i].original_color = enemies[i].sprite.color;
            // 初始化被击中颜色阶段索引
            enemies[i].hit_color_index = 0;
            
            // 根据敌人成功袭击次数设置最低速度
            int min_speed = 1;
            if (enemy_hit_count >= 5) {
                min_speed = 3; // 被袭击5次后，最低速度为3
            } else if (enemy_hit_count >= 3) {
                min_speed = 2; // 被袭击3次后，最低速度为2
            }
            
            // 设置移动速度（min_speed到5）
            enemies[i].speed = (SYS_Random() % (6 - min_speed)) + min_speed;
            
            // 从屏幕边缘随机位置生成，避开四个角落的20x20区域
            int edge = SYS_Random() % 4; // 0=上，1=右，2=下，3=左
            int spawn_x, spawn_y;
            int corner_size = CORNER_SIZE; // 使用预定义的角落区域大小

            switch (edge) {
                case 0: // 上边缘
                    do {
                        // 确保不在左上角或右上角的20x20区域内
                        spawn_x = SYS_Random() % (WIDTH - enemies[i].sprite.w);
                        spawn_y = 1;
                    } while ((spawn_x < corner_size) || // 避开左上角
                             (spawn_x + enemies[i].sprite.w > WIDTH - corner_size)); // 避开右上角
                    break;
                case 1: // 右边缘
                    do {
                        // 确保不在右上角或右下角的20x20区域内
                        spawn_x = WIDTH - enemies[i].sprite.w - 1;
                        spawn_y = SYS_Random() % (HEIGHT - enemies[i].sprite.h);
                    } while ((spawn_y < corner_size) || // 避开右上角
                             (spawn_y + enemies[i].sprite.h > HEIGHT - corner_size)); // 避开右下角
                    break;
                case 2: // 下边缘
                    do {
                        // 确保不在左下角或右下角的20x20区域内
                        spawn_x = SYS_Random() % (WIDTH - enemies[i].sprite.w);
                        spawn_y = HEIGHT - enemies[i].sprite.h - 1;
                    } while ((spawn_x < corner_size) || // 避开左下角
                             (spawn_x + enemies[i].sprite.w > WIDTH - corner_size)); // 避开右下角
                    break;
                case 3: // 左边缘
                    do {
                        // 确保不在左上角或左下角的20x20区域内
                        spawn_x = 1;
                        spawn_y = SYS_Random() % (HEIGHT - enemies[i].sprite.h);
                    } while ((spawn_y < corner_size) || // 避开左上角
                             (spawn_y + enemies[i].sprite.h > HEIGHT - corner_size)); // 避开左下角
                    break;
            }
            
            enemies[i].world_x = spawn_x;
            enemies[i].world_y = spawn_y;
            
            // 根据滚动偏移量计算屏幕坐标
            enemies[i].sprite.screen_x = enemies[i].world_x - (scrollX % GRID_SIZE);
            enemies[i].sprite.screen_y = enemies[i].world_y;

            // 设置初始移动方向，确保敌人向屏幕内部移动
            int min_inward_speed = 1; // 至少向内移动的速度
            enemies[i].dx = 0;
            enemies[i].dy = 0;

            // 检查左边界
            if (enemies[i].world_x < CORNER_SIZE) {
                enemies[i].dx = min_inward_speed;
            }
            // 检查右边界
            if (enemies[i].world_x + enemies[i].sprite.w > WIDTH - CORNER_SIZE) {
                enemies[i].dx = -min_inward_speed;
            }
            // 检查上边界
            if (enemies[i].world_y < CORNER_SIZE) {
                enemies[i].dy = min_inward_speed;
            }
            // 检查下边界
            if (enemies[i].world_y + enemies[i].sprite.h > HEIGHT - CORNER_SIZE) {
                enemies[i].dy = -min_inward_speed;
            }
            
            // 如果敌人生成在角落，并且dx或dy为0，则强制一个方向
            if (enemies[i].dx == 0 && enemies[i].dy == 0) {
                // 随机选择一个方向
                if (SYS_Random() % 2 == 0) {
                    enemies[i].dx = (SYS_Random() % 2 == 0) ? min_inward_speed : -min_inward_speed;
                } else {
                    enemies[i].dy = (SYS_Random() % 2 == 0) ? min_inward_speed : -min_inward_speed;
                }
            }

            // 初始化曲线移动计数器
            enemies[i].curve_counter = 0;
            
            // 初始化移动距离
            enemies[i].distance_traveled = 0;
            
            // 初始化静止计时器和上一帧位置
            enemies[i].stationary_timer = 0;
            enemies[i].prev_x = enemies[i].world_x;
            enemies[i].prev_y = enemies[i].world_y;
            
            // 转换为真实坐标
            screen_to_real_coords(&enemies[i].sprite);
            
            // 激活敌人并设置为进入状态
            enemies[i].active = 1;
            enemies[i].state = ENEMY_STATE_ENTERING; // 设置为进入状态
            active_enemy_count++;
            
            break;
        }
    }
}

/* 发射子弹 */
static void shoot_bullets(int direction)
{
    int i;
    
    // 检查射击是否被禁用
    if (shooting_disabled) {
        return;
    }
    
    // 检查活跃子弹数量是否已达到上限
    if (active_bullet_count >= 4) {
        return;
    }

    // 根据方向发射子弹
    if (direction == 0) {  // A键：左右发射
  sound_set_timer(1);
  sound_on(1, 2000, 63, 1);
        // 左侧子弹
        for (i = 0; i < MAX_BULLETS; i++) {
            if (!bullets[i].active) {
                bullets[i].active = 1;
                bullets[i].sprite.screen_x = playerSprite.screen_x - 6;
                bullets[i].sprite.screen_y = playerSprite.screen_y;
                bullets[i].dx = -10;  // 每帧10像素
                bullets[i].dy = 0;
                bullets[i].sprite.color = playerSprite.color;
                screen_to_real_coords(&bullets[i].sprite);
                active_bullet_count++; // 增加活跃子弹计数
                break;
            }
        }
        
        // 右侧子弹
        for (i = 0; i < MAX_BULLETS; i++) {
            if (!bullets[i].active) {
                bullets[i].active = 1;
                bullets[i].sprite.screen_x = playerSprite.screen_x + playerSprite.w;
                bullets[i].sprite.screen_y = playerSprite.screen_y;
                bullets[i].dx = 10;   // 每帧10像素
                bullets[i].dy = 0;
                bullets[i].sprite.color = playerSprite.color;
                screen_to_real_coords(&bullets[i].sprite);
                active_bullet_count++; // 增加活跃子弹计数
                break;
            }
        }
    } else if (direction == 1) {  // B键：上下发射
  sound_set_timer(1);
  sound_on(1, 1900, 63, 3);
        // 上侧子弹
        for (i = 0; i < MAX_BULLETS; i++) {
            if (!bullets[i].active) {
                bullets[i].active = 1;
                bullets[i].sprite.screen_x = playerSprite.screen_x;
                bullets[i].sprite.screen_y = playerSprite.screen_y - 6;
                bullets[i].dx = 0;
                bullets[i].dy = -10;  // 每帧10像素
                bullets[i].sprite.color = playerSprite.color;
                screen_to_real_coords(&bullets[i].sprite);
                active_bullet_count++; // 增加活跃子弹计数
                break;
            }
        }
        
        // 下侧子弹
        for (i = 0; i < MAX_BULLETS; i++) {
            if (!bullets[i].active) {
                bullets[i].active = 1;
                bullets[i].sprite.screen_x = playerSprite.screen_x;
                bullets[i].sprite.screen_y = playerSprite.screen_y + playerSprite.h;
                bullets[i].dx = 0;
                bullets[i].dy = 10;   // 每帧10像素
                bullets[i].sprite.color = playerSprite.color;
                screen_to_real_coords(&bullets[i].sprite);
                active_bullet_count++; // 增加活跃子弹计数
                break;
            }
        }
    }
}

/* 更新子弹位置 */
static void update_bullets()
{
    int i;
    for (i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            // 保存旧位置
            Sprite prev_bullet = bullets[i].sprite;
            
            // 更新屏幕相对位置
            bullets[i].sprite.screen_x += bullets[i].dx;
            bullets[i].sprite.screen_y += bullets[i].dy;
            
            // 转换为真实坐标
            screen_to_real_coords(&bullets[i].sprite);
            
            // 检查边界：子弹接触到屏幕边缘时自动消失
            // 屏幕坐标x=0到159，y=0到119
            if (bullets[i].sprite.screen_x < 0 ||
                bullets[i].sprite.screen_x + bullets[i].sprite.w > WIDTH ||
                bullets[i].sprite.screen_y < 0 ||
                bullets[i].sprite.screen_y + bullets[i].sprite.h > HEIGHT) {
                bullets[i].active = 0;
                active_bullet_count--; // 减少活跃子弹计数
                // 恢复背景
                restore_sprite_background(&prev_bullet, &bullets[i].sprite, 1);
            } else {
                // 恢复旧位置的背景
                restore_sprite_background(&prev_bullet, &bullets[i].sprite, 0);
            }
        }
    }
}

/* 绘制所有活跃的子弹 */
static void draw_bullets()
{
    int i;
    for (i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            // 检查子弹是否在可见屏幕范围内
            if (bullets[i].sprite.screen_x >= 0 &&
                bullets[i].sprite.screen_x <= WIDTH - bullets[i].sprite.w &&
                bullets[i].sprite.screen_y >= 0 &&
                bullets[i].sprite.screen_y <= HEIGHT - bullets[i].sprite.h) {
                draw_sprite(&bullets[i].sprite);
            }
        }
    }
}

/* 更新敌人位置 */
static void update_enemies()
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            // 保存旧位置
            Sprite prev_enemy = enemies[i].sprite;
            
            // 将这些变量的声明移到函数顶部，避免重定义
            int enemy_left = enemies[i].world_x;
            int enemy_right = enemies[i].world_x + enemies[i].sprite.w;
            int enemy_top = enemies[i].world_y;
            int enemy_bottom = enemies[i].world_y + enemies[i].sprite.h;

            int has_left_border = (enemy_left >= CORNER_SIZE && enemy_right <= WIDTH - CORNER_SIZE &&
                                   enemy_top >= CORNER_SIZE && enemy_bottom <= HEIGHT - CORNER_SIZE);
            
            // 检查敌人是否静止（0.5秒 = 30帧，假设60fps）
            int is_stationary = (enemies[i].world_x == enemies[i].prev_x && enemies[i].world_y == enemies[i].prev_y);
            
            if (is_stationary) {
                enemies[i].stationary_timer++;
                // 如果静止超过0.5秒（30帧），随机分配新的行为模式
                if (enemies[i].stationary_timer >= 30) {
                    enemies[i].move_type = SYS_Random() % 3; // 随机选择移动类型
                    
                    // 根据新的move_type设置初始dx, dy
                    switch (enemies[i].move_type) {
                        case 0: // 直线移动
                            enemies[i].dx = (SYS_Random() % 3 - 1) * enemies[i].speed;
                            enemies[i].dy = (SYS_Random() % 3 - 1) * enemies[i].speed;
                            // 确保至少有一个方向不为0
                            if (enemies[i].dx == 0 && enemies[i].dy == 0) {
                                enemies[i].dx = (SYS_Random() % 2 == 0) ? enemies[i].speed : -enemies[i].speed;
                            }
                            break;
                        case 1: // 随机曲线移动
                            enemies[i].dx = (SYS_Random() % 3 - 1) * enemies[i].speed;
                            enemies[i].dy = (SYS_Random() % 3 - 1) * enemies[i].speed;
                            // 确保至少有一个方向不为0
                            if (enemies[i].dx == 0 && enemies[i].dy == 0) {
                                enemies[i].dx = (SYS_Random() % 2 == 0) ? enemies[i].speed : -enemies[i].speed;
                            }
                            enemies[i].curve_counter = 0;
                            break;
                        case 2: // 追击玩家
                            {
                                int dx = playerSprite.screen_x - enemies[i].sprite.screen_x;
                                int dy = playerSprite.screen_y - enemies[i].sprite.screen_y;
                                int length = abs(dx) + abs(dy);
                                if (length > 0) {
                                    enemies[i].dx = (dx * enemies[i].speed) / length;
                                    enemies[i].dy = (dy * enemies[i].speed) / length;
                                } else {
                                    // 如果与玩家重合，随机选择一个方向
                                    enemies[i].dx = (SYS_Random() % 2 == 0) ? enemies[i].speed : -enemies[i].speed;
                                    enemies[i].dy = (SYS_Random() % 2 == 0) ? enemies[i].speed : -enemies[i].speed;
                                }
                            }
                            break;
                    }
                    
                    // 重置静止计时器
                    enemies[i].stationary_timer = 0;
                }
            } else {
                // 敌人移动了，重置静止计时器
                enemies[i].stationary_timer = 0;
            }
            
            // 保存当前位置作为下一帧的上一帧位置
            enemies[i].prev_x = enemies[i].world_x;
            enemies[i].prev_y = enemies[i].world_y;

            if (enemies[i].state == ENEMY_STATE_ENTERING) {
                if (has_left_border) {
                    // 敌人已离开边框区域，切换到活跃状态并随机分配行为
                    enemies[i].state = ENEMY_STATE_ACTIVE;
                    enemies[i].move_type = SYS_Random() % 3; // 随机选择移动类型
                    
                    // 根据新的move_type设置初始dx, dy
                    switch (enemies[i].move_type) {
                        case 0: // 直线移动
                            // 保持当前的dx, dy，或者重新随机一个方向
                            enemies[i].dx = (SYS_Random() % 3 - 1) * enemies[i].speed;
                            enemies[i].dy = (SYS_Random() % 3 - 1) * enemies[i].speed;
                            break;
                        case 1: // 随机曲线移动
                            enemies[i].dx = (SYS_Random() % 3 - 1) * enemies[i].speed;
                            enemies[i].dy = (SYS_Random() % 3 - 1) * enemies[i].speed;
                            enemies[i].curve_counter = 0;
                            break;
                        case 2: // 追击玩家
                            // 计算朝向玩家的方向
                            {
                                int dx = playerSprite.screen_x - enemies[i].sprite.screen_x;
                                int dy = playerSprite.screen_y - enemies[i].sprite.screen_y;
                                int length = abs(dx) + abs(dy);
                                if (length > 0) {
                                    enemies[i].dx = (dx * enemies[i].speed) / length;
                                    enemies[i].dy = (dy * enemies[i].speed) / length;
                                }
                            }
                            break;
                    }
                }
            }
            
            // 根据移动类型更新世界坐标
            // 只有当敌人处于活跃状态时才执行其行为模式
            if (enemies[i].state == ENEMY_STATE_ACTIVE) {
                switch (enemies[i].move_type) {
                    case 0: // 直线移动
                        enemies[i].world_x += enemies[i].dx;
                        enemies[i].world_y += enemies[i].dy;
                        break;
                        
                    case 1: // 随机曲线移动
                        enemies[i].curve_counter++;
                        // 每隔一段时间改变方向
                        if (enemies[i].curve_counter % 20 == 0) {
                            enemies[i].dx = (SYS_Random() % 3 - 1) * enemies[i].speed;
                            enemies[i].dy = (SYS_Random() % 3 - 1) * enemies[i].speed;
                        }
                        enemies[i].world_x += enemies[i].dx;
                        enemies[i].world_y += enemies[i].dy;
                        break;
                        
                    case 2: // 追击玩家
                        {
                            // 计算朝向玩家的方向 (基于玩家的屏幕坐标，因为玩家是相对屏幕静止的)
                            int dx = playerSprite.screen_x - enemies[i].sprite.screen_x;
                            int dy = playerSprite.screen_y - enemies[i].sprite.screen_y;
                            
                            // 归一化方向向量并应用速度
                            int length = abs(dx) + abs(dy); // 简化的距离计算
                            if (length > 0) {
                                enemies[i].dx = (dx * enemies[i].speed) / length;
                                enemies[i].dy = (dy * enemies[i].speed) / length;
                            }
                            
                            enemies[i].world_x += enemies[i].dx;
                            enemies[i].world_y += enemies[i].dy;
                        }
                        break;
                }
            } else { // ENEMY_STATE_ENTERING 状态，继续向内移动
                enemies[i].world_x += enemies[i].dx;
                enemies[i].world_y += enemies[i].dy;
            }
            
            // 更新移动后的位置，检查是否进入角落区域
            enemy_left = enemies[i].world_x;
            enemy_right = enemies[i].world_x + enemies[i].sprite.w;
            enemy_top = enemies[i].world_y;
            enemy_bottom = enemies[i].world_y + enemies[i].sprite.h;
            
            // 检查是否进入角落区域，如果是则强制调整位置
            int is_in_top_left_corner = (enemy_left < CORNER_SIZE && enemy_top < CORNER_SIZE);
            int is_in_top_right_corner = (enemy_right > WIDTH - CORNER_SIZE && enemy_top < CORNER_SIZE);
            int is_in_bottom_left_corner = (enemy_left < CORNER_SIZE && enemy_bottom > HEIGHT - CORNER_SIZE);
            int is_in_bottom_right_corner = (enemy_right > WIDTH - CORNER_SIZE && enemy_bottom > HEIGHT - CORNER_SIZE);
            
            // 如果进入任何角落区域，强制调整位置
            if (is_in_top_left_corner) {
                // 左上角：强制向右或向下移动
                if (enemy_left < CORNER_SIZE / 2 && enemy_top < CORNER_SIZE / 2) {
                    // 在角落中心，随机选择一个方向
                    if (SYS_Random() % 2 == 0) {
                        enemies[i].world_x = CORNER_SIZE;
                        enemies[i].dx = abs(enemies[i].dx); // 确保向右移动
                    } else {
                        enemies[i].world_y = CORNER_SIZE;
                        enemies[i].dy = abs(enemies[i].dy); // 确保向下移动
                    }
                } else if (enemy_left < CORNER_SIZE) {
                    // 主要在左侧，向右移动
                    enemies[i].world_x = CORNER_SIZE;
                    enemies[i].dx = abs(enemies[i].dx);
                } else {
                    // 主要在顶部，向下移动
                    enemies[i].world_y = CORNER_SIZE;
                    enemies[i].dy = abs(enemies[i].dy);
                }
            } else if (is_in_top_right_corner) {
                // 右上角：强制向左或向下移动
                if (enemy_right > WIDTH - CORNER_SIZE / 2 && enemy_top < CORNER_SIZE / 2) {
                    // 在角落中心，随机选择一个方向
                    if (SYS_Random() % 2 == 0) {
                        enemies[i].world_x = WIDTH - CORNER_SIZE - enemies[i].sprite.w;
                        enemies[i].dx = -abs(enemies[i].dx); // 确保向左移动
                    } else {
                        enemies[i].world_y = CORNER_SIZE;
                        enemies[i].dy = abs(enemies[i].dy); // 确保向下移动
                    }
                } else if (enemy_right > WIDTH - CORNER_SIZE) {
                    // 主要在右侧，向左移动
                    enemies[i].world_x = WIDTH - CORNER_SIZE - enemies[i].sprite.w;
                    enemies[i].dx = -abs(enemies[i].dx);
                } else {
                    // 主要在顶部，向下移动
                    enemies[i].world_y = CORNER_SIZE;
                    enemies[i].dy = abs(enemies[i].dy);
                }
            } else if (is_in_bottom_left_corner) {
                // 左下角：强制向右或向上移动
                if (enemy_left < CORNER_SIZE / 2 && enemy_bottom > HEIGHT - CORNER_SIZE / 2) {
                    // 在角落中心，随机选择一个方向
                    if (SYS_Random() % 2 == 0) {
                        enemies[i].world_x = CORNER_SIZE;
                        enemies[i].dx = abs(enemies[i].dx); // 确保向右移动
                    } else {
                        enemies[i].world_y = HEIGHT - CORNER_SIZE - enemies[i].sprite.h;
                        enemies[i].dy = -abs(enemies[i].dy); // 确保向上移动
                    }
                } else if (enemy_left < CORNER_SIZE) {
                    // 主要在左侧，向右移动
                    enemies[i].world_x = CORNER_SIZE;
                    enemies[i].dx = abs(enemies[i].dx);
                } else {
                    // 主要在底部，向上移动
                    enemies[i].world_y = HEIGHT - CORNER_SIZE - enemies[i].sprite.h;
                    enemies[i].dy = -abs(enemies[i].dy);
                }
            } else if (is_in_bottom_right_corner) {
                // 右下角：强制向左或向上移动
                if (enemy_right > WIDTH - CORNER_SIZE / 2 && enemy_bottom > HEIGHT - CORNER_SIZE / 2) {
                    // 在角落中心，随机选择一个方向
                    if (SYS_Random() % 2 == 0) {
                        enemies[i].world_x = WIDTH - CORNER_SIZE - enemies[i].sprite.w;
                        enemies[i].dx = -abs(enemies[i].dx); // 确保向左移动
                    } else {
                        enemies[i].world_y = HEIGHT - CORNER_SIZE - enemies[i].sprite.h;
                        enemies[i].dy = -abs(enemies[i].dy); // 确保向上移动
                    }
                } else if (enemy_right > WIDTH - CORNER_SIZE) {
                    // 主要在右侧，向左移动
                    enemies[i].world_x = WIDTH - CORNER_SIZE - enemies[i].sprite.w;
                    enemies[i].dx = -abs(enemies[i].dx);
                } else {
                    // 主要在底部，向上移动
                    enemies[i].world_y = HEIGHT - CORNER_SIZE - enemies[i].sprite.h;
                    enemies[i].dy = -abs(enemies[i].dy);
                }
            }
            
            // 更新移动距离
            enemies[i].distance_traveled += abs(enemies[i].dx) + abs(enemies[i].dy);

            // 如果屏幕正在滚动，调整敌人的世界坐标以抵消滚动，使其在屏幕上的相对位置保持不变
            if (scrollDirection != 0) {
                enemies[i].world_x -= scrollDirection; // 与scrollDirection方向相反
            }
            
            // 敌人的屏幕相对坐标直接等于其世界坐标
            enemies[i].sprite.screen_x = enemies[i].world_x;
            enemies[i].sprite.screen_y = enemies[i].world_y;
            
            // 转换为真实坐标
            screen_to_real_coords(&enemies[i].sprite);
            
            // 检查边界：敌人必须在屏幕内移动至少20像素才会离开屏幕
            enemy_left = enemies[i].world_x;
            enemy_right = enemies[i].world_x + enemies[i].sprite.w;
            enemy_top = enemies[i].world_y;
            enemy_bottom = enemies[i].world_y + enemies[i].sprite.h;

            int is_in_top_left_corner_area = (enemy_left < CORNER_SIZE && enemy_top < CORNER_SIZE);
            int is_in_top_right_corner_area = (enemy_right > WIDTH - CORNER_SIZE && enemy_top < CORNER_SIZE);
            int is_in_bottom_left_corner_area = (enemy_left < CORNER_SIZE && enemy_bottom > HEIGHT - CORNER_SIZE);
            int is_in_bottom_right_corner_area = (enemy_right > WIDTH - CORNER_SIZE && enemy_bottom > HEIGHT - CORNER_SIZE);

            int is_leaving_screen = (enemy_left < 0 || enemy_right > WIDTH || enemy_top < 0 || enemy_bottom > HEIGHT);

            // 检查敌人是否试图离开屏幕
            if (enemy_left < 0 || enemy_right > WIDTH || enemy_top < 0 || enemy_bottom > HEIGHT) {
                // 如果在角落区域，则强制反弹
                if (is_in_top_left_corner_area || is_in_top_right_corner_area || is_in_bottom_left_corner_area || is_in_bottom_right_corner_area) {
                    // 钳制位置以保持在屏幕内
                    if (enemy_left < 0) enemies[i].world_x = 0;
                    if (enemy_right > WIDTH) enemies[i].world_x = WIDTH - enemies[i].sprite.w;
                    if (enemy_top < 0) enemies[i].world_y = 0;
                    if (enemy_bottom > HEIGHT) enemies[i].world_y = HEIGHT - enemies[i].sprite.h;
                    // 反转方向
                    enemies[i].dx = -enemies[i].dx;
                    enemies[i].dy = -enemies[i].dy;
                } else {
                    // 如果不在角落区域，并且已经移动了足够距离，则销毁敌人
                    if (enemies[i].distance_traveled >= 20) {
                        enemies[i].active = 0;
                        active_enemy_count--;
                        restore_sprite_background(&prev_enemy, &enemies[i].sprite, 1);
                    } else {
                        // 如果不在角落区域，但未移动足够距离，则钳制到屏幕边缘并反转方向
                        // 这可以防止在未移动20像素时过早销毁
                        if (enemy_left < 0) enemies[i].world_x = 0;
                        if (enemy_right > WIDTH) enemies[i].world_x = WIDTH - enemies[i].sprite.w;
                        if (enemy_top < 0) enemies[i].world_y = 0;
                        if (enemy_bottom > HEIGHT) enemies[i].world_y = HEIGHT - enemies[i].sprite.h;
                        enemies[i].dx = -enemies[i].dx;
                        enemies[i].dy = -enemies[i].dy;
                    }
                }
            } else {
                // 如果没有离开屏幕，则恢复背景
                restore_sprite_background(&prev_enemy, &enemies[i].sprite, 0);
            }
        }
    }
}

/* 绘制所有活跃的敌人 */
static void draw_enemies()
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            // 检查敌人是否在可见屏幕范围内
            if (enemies[i].sprite.screen_x >= 0 &&
                enemies[i].sprite.screen_x <= WIDTH - enemies[i].sprite.w &&
                enemies[i].sprite.screen_y >= 0 &&
                enemies[i].sprite.screen_y <= HEIGHT - enemies[i].sprite.h) {
                draw_sprite(&enemies[i].sprite);
            }
        }
    }
}

/* 检查子弹和敌人的碰撞 */
static void check_bullet_enemy_collisions()
{
    int i, j;
    int bullet_hit = 0; // 标记是否有子弹击中敌人
    
    for (i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            for (j = 0; j < MAX_ENEMIES; j++) {
                if (enemies[j].active) {
                    // 简单的矩形碰撞检测
                    if (bullets[i].sprite.screen_x < enemies[j].sprite.screen_x + enemies[j].sprite.w &&
                        bullets[i].sprite.screen_x + bullets[i].sprite.w > enemies[j].sprite.screen_x &&
                        bullets[i].sprite.screen_y < enemies[j].sprite.screen_y + enemies[j].sprite.h &&
                        bullets[i].sprite.screen_y + bullets[i].sprite.h > enemies[j].sprite.screen_y) {
                        
                        // 碰撞发生，减少敌人血量
                        enemies[j].health--;
                        
                        // 销毁子弹
                        bullets[i].active = 0;
                        active_bullet_count--;
                        
                        // 标记有子弹击中敌人
                        bullet_hit = 1;
                        
                        // AI学习：击中敌人加分
                        if (demo_mode && ai_learning_enabled && last_behavior_type >= 0) {
                            // 修改规则：发射子弹不扣分，只有长时间不发射子弹才扣分
                            behavior_scores[last_behavior_type] += hit_reward;
                            behavior_attempts[last_behavior_type]++;
                            last_shoot_result = 1; // 标记为击中
                            consecutive_misses = 0; // 重置连续未击中次数
                            
                            // 额外奖励：如果这次射击是随机的，给予额外奖励
                            if (SYS_Random() % 3 == 0) { // 33%概率认为是随机射击
                                behavior_scores[last_behavior_type] += random_shoot_bonus;
                            }
                        }
                        
                        // 敌人被击中后颜色阶段性变化
                        enemies[j].hit_color_index++;
                        if (enemies[j].hit_color_index >= MAX_ENEMY_HIT_COLORS) {
                            enemies[j].hit_color_index = MAX_ENEMY_HIT_COLORS - 1; // 达到最大阶段后保持最后一个颜色
                        }
                        
                        // 设置敌人颜色，确保不与玩家颜色相同
                        int new_color = enemy_hit_colors[enemies[j].hit_color_index];
                        if (new_color == playerSprite.color) {
                            // 如果新颜色与玩家颜色相同，尝试下一个颜色
                            enemies[j].hit_color_index++;
                            if (enemies[j].hit_color_index >= MAX_ENEMY_HIT_COLORS) {
                                enemies[j].hit_color_index = 0; // 循环到第一个颜色
                            }
                            new_color = enemy_hit_colors[enemies[j].hit_color_index];
                        }
                        enemies[j].sprite.color = new_color;
                        
                        // 如果敌人血量为0，销毁敌人并擦除
                        if (enemies[j].health <= 0) {
                            // 擦除敌人
                            restore_sprite_background(&enemies[j].sprite, &enemies[j].sprite, 1);
                            enemies[j].active = 0;
                            active_enemy_count--;
                            
                            // 增加玩家击杀计数
                            player_kill_count++;
                            player_total_kills++; // 新增：更新总击杀数

                            // 难度提升：每击杀5个敌人增加难度1，活动敌人数量加1，最多5
                            enemies_killed_for_difficulty++;
                            if (enemies_killed_for_difficulty >= 5) {
                                enemies_killed_for_difficulty = 0; // 重置计数
                                if (current_difficulty < 5) { // 难度最多为5
                                    current_difficulty++;
                                    if (max_active_enemies < 7) { // 活动敌人数量最多为7
                                        max_active_enemies++;
                                    }
                                }
                            }

                            // 奖励：每击杀5个敌人增加生命1并且恢复颜色
                            if (player_total_kills % 5 == 0) {
                                player_lives++; // 增加生命
                                // 恢复颜色到初始状态
                                current_player_color_index = 0;
                                playerSprite.color = player_colors[current_player_color_index];
                            }
                            
                            // 每击杀2个敌人，速度增加1，但不大于7
                            if (player_kill_count % 2 == 0) {
                                player_speed++;
                                if (player_speed > 7) {
                                    player_speed = 7;
                                }
                            }
                        } else {
                            // 即使敌人没有死亡，也要擦除当前位置
                            restore_sprite_background(&enemies[j].sprite, &enemies[j].sprite, 1);
                        }
                        
                        // 播放击中音效
                        sound_set_timer(1);
                        sound_on(1, 1500, 63, 0);
                        
                        break; // 一个子弹只能击中一个敌人
                    }
                }
            }
        }
    }
    
    // AI学习：恢复未击中扣分机制，但保持基本流程
    if (demo_mode && ai_learning_enabled) {
        // 增加射击频率计时器
        static int shoot_frequency_timer = 0;
        static int last_shoot_frame = 0;
        
        // 如果这一帧发射了子弹，给予奖励
        if (last_shoot_frame != frame_counter) {
            if (last_behavior_type >= 0) {
                behavior_scores[last_behavior_type] += shoot_frequency_bonus;
                behavior_attempts[last_behavior_type]++;
                last_shoot_frame = frame_counter;
            }
        }
        
        // 长时间不射击给予惩罚
        shoot_frequency_timer++;
        if (shoot_frequency_timer >= 60) { // 1秒不射击
            if (last_behavior_type >= 0) {
                behavior_scores[last_behavior_type] -= 2; // 惩罚
                behavior_attempts[last_behavior_type]++;
                shoot_frequency_timer = 0;
            }
        }
        
        // 如果上一帧射击了但未击中，扣分
        if (last_shoot_result == -1) {
            if (last_behavior_type >= 0) {
                behavior_scores[last_behavior_type] -= miss_penalty;
                behavior_attempts[last_behavior_type]++;
            }
        }
    }
}

/* 检查玩家和敌人的碰撞 */
static int check_player_enemy_collision()
{
    int i;
    
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            // 简单的矩形碰撞检测
            if (playerSprite.screen_x < enemies[i].sprite.screen_x + enemies[i].sprite.w &&
                playerSprite.screen_x + playerSprite.w > enemies[i].sprite.screen_x &&
                playerSprite.screen_y < enemies[i].sprite.screen_y + enemies[i].sprite.h &&
                playerSprite.screen_y + playerSprite.h > enemies[i].sprite.screen_y) {
                
                // 碰撞发生，玩家失去一条生命
                player_lives--;
                
                // 增加敌人成功袭击次数
                enemy_hit_count++;
                
                // 主角速度减小1，但不小于1
                player_speed--;
                if (player_speed < 1) {
                    player_speed = 1;
                }
                
                // 切换玩家颜色
                current_player_color_index++;
                if (current_player_color_index >= 3) {
                    current_player_color_index = 0;
                }
                playerSprite.color = player_colors[current_player_color_index];
                
                // 播放受伤音效
                sound_set_timer(1);
                sound_on(1, 800, 63, 0);
                
                // 擦除并销毁碰撞的敌人
                restore_sprite_background(&enemies[i].sprite, &enemies[i].sprite, 1);
                enemies[i].active = 0;
                active_enemy_count--;
                
                return 1; // 返回碰撞发生
            }
        }
    }
    
    return 0; // 没有碰撞
}

/* 重置游戏 */
static void reset_game()
{
    // 重置玩家位置和生命值
    init_sprite(&playerSprite, WIDTH / 2 - 3, HEIGHT / 2 - 3, 6, 6, 0x3F);
    player_lives = 3;
    current_player_color_index = 0;
    playerSprite.color = player_colors[current_player_color_index];
    
    // 重置玩家速度和击杀计数
    player_speed = 3;
    player_kill_count = 0;
    
    // 重置子弹系统
    init_bullets();
    
    // 重置敌人系统
    init_enemies();
    
    // 重置敌人成功袭击次数
    enemy_hit_count = 0;
    
    // 新增：重置难度和击杀相关变量
    current_difficulty = 1;
    enemies_killed_for_difficulty = 0;
    max_active_enemies = 1;
    player_total_kills = 0;
    
    // 重置滚动相关变量
    scrollX = 0;
    scrollDirection = 0;
    *ShiftControl = 0;
    total_scrolled_distance = 0;
    enemy_spawn_frequency_divisor = 1;
    enemy_spawn_max_count_increase = 0;
    auto_scroll_active = 0;
    auto_scroll_target_distance = 0;
    auto_scroll_current_distance = 0;
    auto_scroll_direction = 0;
    auto_scroll_preparing = 0;
    shooting_disabled = 0;
    
    // 重置演示模式相关变量，但不重置demo_mode标志，实现演示模式持久化
    // demo_mode = 0; // 注释掉，不重置演示模式标志
    demo_move_timer = 0;
    demo_shoot_timer = 0;
    demo_direction = 0;
    demo_target_enemy = -1;
    demo_surrounded = 0;
    demo_surrounded_timer = 0;
    demo_aim_timer = 0;
    demo_stable_direction_timer = 0;
    demo_last_direction = -1;
    demo_aimed_shot = 0;
    demo_enemy_predict_x = 0;
    demo_enemy_predict_y = 0;
    demo_rapid_fire_timer = 0;
    demo_horizontal_target = -1;
    
    // 重置新增的斜线移动相关变量
    demo_diagonal_move = 0;
    demo_dx = 0;
    demo_dy = 0;
    demo_can_shoot_while_moving = 1;
    
    // 初始化AI行为打分系统
    int i;
    for (i = 0; i < MAX_BEHAVIOR_TYPES; i++) {
        behavior_scores[i] = 0;
        behavior_attempts[i] = 0;
    }
    last_behavior_type = -1;
    last_shoot_result = 0;
    consecutive_misses = 0;
    ai_learning_enabled = 1;
    
    // 重置主角子弹发射计时器
    player_shoot_timer = 0;
    
    // 重绘背景
    bg();
    
    // 播放重置音效
    sound_set_timer(1);
    sound_on(1, 1000, 63, 10);
}

/* 演示模式下的自动移动逻辑 */
static void demo_auto_move(Sprite *sprite)
{
    // 简化移动延迟计算
    int move_delay = (sprite->w + sprite->h) / 20;
    if (move_delay < 1) move_delay = 1;
    if (move_delay > 3) move_delay = 3;
    
    // 只有当帧计数器达到移动延迟时才处理移动
    if (frame_counter % move_delay == 0) {
        demo_move_timer++;
        demo_stable_direction_timer++;
        
        // AI学习：选择最佳行为类型
        if (ai_learning_enabled && demo_move_timer % 20 == 0) { // 每20帧重新评估一次
            int best_behavior = select_best_behavior();
            apply_behavior(best_behavior);
        }
        
        // 定义屏幕中心区域
        int center_x = WIDTH / 2;
        int center_y = HEIGHT / 2;
        int center_range = 30; // 中心区域范围
        
        // 检查是否在中心区域内
        int in_center_x = (abs(sprite->screen_x - center_x) < center_range);
        int in_center_y = (abs(sprite->screen_y - center_y) < center_range);
        int in_center = in_center_x && in_center_y;
        
        // 减少抖动：只有在必要时才改变方向
        int should_change_direction = 0;
        
        // 如果正在瞄准，尽量保持稳定，并使用预判位置
        if (demo_aimed_shot && demo_target_enemy != -1) {
            // 瞄准时，根据敌人预判位置微调方向，减少大幅改变
            Enemy *target = &enemies[demo_target_enemy];
            
            // 使用预判位置进行瞄准
            int target_dx = demo_enemy_predict_x - sprite->screen_x;
            int target_dy = demo_enemy_predict_y - sprite->screen_y;
            
            // 计算方向一致性，考虑预判位置
            // 如果方向不匹配且已经稳定一段时间，调整方向以更好地瞄准预判位置
            if (demo_stable_direction_timer > 15) {
                // 启用斜线移动，直接计算朝向目标的向量
                demo_diagonal_move = 1;
                
                // 计算朝向目标的单位向量
                int length = abs(target_dx) + abs(target_dy);
                if (length > 0) {
                    // 计算速度分量，保持总速度约为player_speed
                    if (abs(target_dx) > abs(target_dy)) {
                        // 水平距离更大
                        demo_dx = (target_dx > 0) ? player_speed : -player_speed;
                        demo_dy = (target_dy * player_speed) / abs(target_dx);
                    } else {
                        // 垂直距离更大
                        demo_dy = (target_dy > 0) ? player_speed : -player_speed;
                        demo_dx = (target_dx * player_speed) / abs(target_dy);
                    }
                }
                demo_stable_direction_timer = 0;
            }
        } else {
            // 非瞄准状态，正常移动逻辑
            int direction_change_interval = in_center ? 80 : 40; // 增加改变方向的间隔
            
            if (demo_move_timer >= direction_change_interval) {
                demo_move_timer = 0;
                should_change_direction = 1;
            }
        }
        
        if (should_change_direction) {
            // 如果不在中心区域，倾向于向中心移动
            if (!in_center) {
                // 计算朝向中心的方向
                int dx = center_x - sprite->screen_x;
                int dy = center_y - sprite->screen_y;
                
                // 启用斜线移动，直接计算朝向中心的向量
                demo_diagonal_move = 1;
                
                // 计算速度分量，保持总速度约为player_speed
                int length = abs(dx) + abs(dy);
                if (length > 0) {
                    if (abs(dx) > abs(dy)) {
                        // 水平距离更大
                        demo_dx = (dx > 0) ? player_speed : -player_speed;
                        demo_dy = (dy * player_speed) / abs(dx);
                    } else {
                        // 垂直距离更大
                        demo_dy = (dy > 0) ? player_speed : -player_speed;
                        demo_dx = (dx * player_speed) / abs(dy);
                    }
                }
            } else {
                // 在中心区域内，减少随机改变方向的频率
                if (SYS_Random() % 5 == 0) { // 20%概率改变方向，减少抖动
                    // 随机选择一个方向，包括斜线
                    demo_diagonal_move = (SYS_Random() % 3 == 0); // 33%概率使用斜线移动
                    
                    if (demo_diagonal_move) {
                        // 随机斜线方向
                        demo_dx = (SYS_Random() % 3 - 1) * player_speed; // -1, 0, 1
                        demo_dy = (SYS_Random() % 3 - 1) * player_speed; // -1, 0, 1
                        
                        // 确保至少有一个方向不为0
                        if (demo_dx == 0 && demo_dy == 0) {
                            demo_dx = (SYS_Random() % 2 == 0) ? player_speed : -player_speed;
                        }
                    } else {
                        // 传统四方向移动
                        demo_direction = (demo_direction + 1) % 4; // 循环方向：0=上，1=右，2=下，3=左
                        demo_dx = 0;
                        demo_dy = 0;
                    }
                }
            }
            
            // 记录方向变化
            if (demo_direction != demo_last_direction) {
                demo_last_direction = demo_direction;
                demo_stable_direction_timer = 0;
            }
        }
        
        // 根据当前方向移动，增加边界检测的平滑性
        int next_x = sprite->screen_x;
        int next_y = sprite->screen_y;
        
        if (demo_diagonal_move) {
            // 斜线移动
            next_x = sprite->screen_x + demo_dx;
            next_y = sprite->screen_y + demo_dy;
            
            // 边界检测和反弹，确保不能进入距离边距10像素的区域
            if (next_x < 10) {
                next_x = 10; // 限制在边界
                demo_dx = -demo_dx; // 反转X方向
            }
            if (next_x > 150 - sprite->w) {
                next_x = 150 - sprite->w; // 限制在边界
                demo_dx = -demo_dx; // 反转X方向
            }
            if (next_y < 10) {
                next_y = 10; // 限制在边界
                demo_dy = -demo_dy; // 反转Y方向
            }
            if (next_y > 110 - sprite->h) {
                next_y = 110 - sprite->h; // 限制在边界
                demo_dy = -demo_dy; // 反转Y方向
            }
        } else {
            // 传统四方向移动
            switch (demo_direction) {
                case 0: // 上
                    if (sprite->screen_y > 10) {
                        next_y = sprite->screen_y - player_speed;
                        // 确保不会越过边界
                        if (next_y < 10) next_y = 10;
                    } else {
                        demo_direction = 2; // 改为向下
                    }
                    break;
                case 1: // 右
                    if (sprite->screen_x < 150 - sprite->w) {
                        next_x = sprite->screen_x + player_speed;
                        // 确保不会越过边界
                        if (next_x > 150 - sprite->w) next_x = 150 - sprite->w;
                    } else {
                        demo_direction = 3; // 改为向左
                    }
                    break;
                case 2: // 下
                    if (sprite->screen_y < 110 - sprite->h) {
                        next_y = sprite->screen_y + player_speed;
                        // 确保不会越过边界
                        if (next_y > 110 - sprite->h) next_y = 110 - sprite->h;
                    } else {
                        demo_direction = 0; // 改为向上
                    }
                    break;
                case 3: // 左
                    if (sprite->screen_x > 10) {
                        next_x = sprite->screen_x - player_speed;
                        // 确保不会越过边界
                        if (next_x < 10) next_x = 10;
                    } else {
                        demo_direction = 1; // 改为向右
                    }
                    break;
            }
        }
        
        // 只有当新位置不会导致抖动时才更新
        int position_change = abs(next_x - sprite->screen_x) + abs(next_y - sprite->screen_y);
        if (position_change > 0) {
            sprite->screen_x = next_x;
            sprite->screen_y = next_y;
        }
        
        // 转换为真实坐标
        screen_to_real_coords(sprite);
    }
}

/* AI学习：选择最佳行为类型 */
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
        
        // 不再因为连续未击中而惩罚行为，鼓励尝试
        // 移除对连续未击中的惩罚
        
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

/* AI学习：根据选择的行为类型设置移动参数 */
static void apply_behavior(int behavior_type)
{
    switch (behavior_type) {
        case 0: // 静止
            demo_dx = 0;
            demo_dy = 0;
            demo_diagonal_move = 0;
            demo_can_shoot_while_moving = 0;
            break;
        case 1: // 向右移动
            demo_dx = player_speed;
            demo_dy = 0;
            demo_diagonal_move = 0;
            demo_can_shoot_while_moving = 1;
            break;
        case 2: // 向左移动
            demo_dx = -player_speed;
            demo_dy = 0;
            demo_diagonal_move = 0;
            demo_can_shoot_while_moving = 1;
            break;
        case 3: // 向下移动
            demo_dx = 0;
            demo_dy = player_speed;
            demo_diagonal_move = 0;
            demo_can_shoot_while_moving = 1;
            break;
        case 4: // 向上移动
            demo_dx = 0;
            demo_dy = -player_speed;
            demo_diagonal_move = 0;
            demo_can_shoot_while_moving = 1;
            break;
        case 5: // 右上斜线移动
            demo_dx = player_speed;
            demo_dy = -player_speed;
            demo_diagonal_move = 1;
            demo_can_shoot_while_moving = 1;
            break;
        case 6: // 右下斜线移动
            demo_dx = player_speed;
            demo_dy = player_speed;
            demo_diagonal_move = 1;
            demo_can_shoot_while_moving = 1;
            break;
        case 7: // 左上斜线移动
            demo_dx = -player_speed;
            demo_dy = -player_speed;
            demo_diagonal_move = 1;
            demo_can_shoot_while_moving = 1;
            break;
        default: // 左下斜线移动（或其他）
            demo_dx = -player_speed;
            demo_dy = player_speed;
            demo_diagonal_move = 1;
            demo_can_shoot_while_moving = 1;
            break;
    }
}

/* 演示模式下的自动攻击逻辑 */
static void demo_auto_shoot()
{
    demo_shoot_timer++;
    demo_aim_timer++;
    
    // 调整射击频率，先瞄准再射击
    int base_shoot_interval = 3; // 基础射击间隔为3帧
    int shoot_interval = base_shoot_interval;
    int aim_required_time = 5; // 瞄准时间为5帧，确保充分瞄准
    
    // 减少随机射击概率，更注重瞄准
    int random_shoot_chance = (SYS_Random() % 10) < 1; // 10%概率进行额外射击
    if (random_shoot_chance) {
        shoot_interval = 1; // 快速射击
    }
    
    // AI学习：根据连续未击中次数决定是否需要移动后再射击
    int should_move_before_shoot = (consecutive_misses >= 2);
    
    // 寻找最佳目标敌人，增强预判能力
    int best_enemy = -1;
    int best_score = -9999;
    int i;
    
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            // 计算与敌人的当前距离
            int dx = enemies[i].sprite.screen_x - playerSprite.screen_x;
            int dy = enemies[i].sprite.screen_y - playerSprite.screen_y;
            int distance = abs(dx) + abs(dy); // 简化的曼哈顿距离
            
            // 预判敌人未来位置（基于敌人移动方向和速度）
            int predict_frames = 5; // 预判5帧后的位置
            int predict_x = enemies[i].sprite.screen_x + enemies[i].dx * predict_frames;
            int predict_y = enemies[i].sprite.screen_y + enemies[i].dy * predict_frames;
            
            // 计算与预判位置的距离
            int predict_dx = predict_x - playerSprite.screen_x;
            int predict_dy = predict_y - playerSprite.screen_y;
            int predict_distance = abs(predict_dx) + abs(predict_dy);
            
            // 保存预判位置供其他函数使用
            if (i == demo_target_enemy) {
                demo_enemy_predict_x = predict_x;
                demo_enemy_predict_y = predict_y;
            }
            
            // 计算得分：距离越近得分越高，血量少的敌人得分更高
            // 同时考虑预判位置，如果预判位置更接近，得分更高
            int score = 100 - distance;
            score += (100 - predict_distance) / 2; // 预判位置的权重为当前距离的一半
            
            if (enemies[i].health == 1) score += 20; // 血量少的敌人优先级更高
            if (distance < 40) score += 30; // 近距离敌人优先级更高
            
            // 如果敌人正在接近玩家，提高优先级
            if (dx * enemies[i].dx + dy * enemies[i].dy < 0) { // 点积为负表示正在接近
                score += 15;
            }
            
            // 如果敌人移动类型是追击玩家，提高优先级
            if (enemies[i].move_type == 2) {
                score += 10;
            }
            
            // 如果使用斜线移动，提高对移动目标的命中率
            if (demo_diagonal_move) {
                score += 5; // 斜线移动时给予额外得分
            }
            
            if (score > best_score) {
                best_score = score;
                best_enemy = i;
            }
        }
    }
    
    // 更新目标敌人
    if (best_enemy != -1) {
        demo_target_enemy = best_enemy;
    }
    
    // 增强瞄准逻辑：考虑子弹体积，精确瞄准敌人中心
    if (demo_target_enemy != -1 && demo_can_shoot_while_moving) {
        Enemy *target = &enemies[demo_target_enemy];
        
        // 使用预判位置进行瞄准，考虑子弹体积（6x6像素）
        int target_x = demo_enemy_predict_x + target->sprite.w / 2; // 敌人中心X坐标
        int target_y = demo_enemy_predict_y + target->sprite.h / 2; // 敌人中心Y坐标
        
        // 计算从玩家中心到敌人中心的向量
        int player_center_x = playerSprite.screen_x + playerSprite.w / 2;
        int player_center_y = playerSprite.screen_y + playerSprite.h / 2;
        
        int dx = target_x - player_center_x;
        int dy = target_y - player_center_y;
        int distance = abs(dx) + abs(dy);
        
        // AI学习：根据连续未击中次数决定是否需要移动后再射击
        if (should_move_before_shoot) {
            // 连续未击中次数过多，倾向于移动后再射击
            demo_aim_timer = 0;
            demo_aimed_shot = 0;
            return; // 不射击，等待移动
        }
        
        // 限制射击范围，更注重瞄准质量
        if (distance < 80) { // 限制在80像素范围内，确保瞄准质量
            // 检查是否已经瞄准足够时间
            if (demo_aim_timer >= aim_required_time) {
                // 瞄准完成，可以射击
                if (demo_shoot_timer >= shoot_interval) {
                    demo_shoot_timer = 0;
                    demo_aim_timer = 0; // 重置瞄准计时器
                    demo_aimed_shot = 1; // 标记为瞄准射击
                    
                    // AI学习：记录当前行为类型
                    int current_behavior_type;
                    if (demo_diagonal_move) {
                        // 斜线移动，根据主要方向分类
                        if (abs(demo_dx) > abs(demo_dy)) {
                            current_behavior_type = (demo_dx > 0) ? 1 : 2; // 右或左
                        } else {
                            current_behavior_type = (demo_dy > 0) ? 3 : 4; // 下或上
                        }
                    } else {
                        // 四方向移动
                        current_behavior_type = demo_direction + 5; // 5-8对应上下左右
                    }
                    
                    // 记录行为类型
                    last_behavior_type = current_behavior_type;
                    last_shoot_result = 0; // 重置射击结果，等待碰撞检测
                    
                    // 根据敌人预判位置选择最优发射方向
                    int shoot_direction;
                    
                    // 更精确的方向判断，考虑子弹体积和飞行时间
                    // 计算子弹到达目标位置需要的时间
                    int bullet_speed = 10; // 子弹速度为10像素/帧
                    int travel_time_x = abs(dx) / bullet_speed;
                    int travel_time_y = abs(dy) / bullet_speed;
                    int max_travel_time = (travel_time_x > travel_time_y) ? travel_time_x : travel_time_y;
                    
                    // 考虑敌人移动，调整瞄准点
                    int adjusted_target_x = target_x + target->dx * max_travel_time;
                    int adjusted_target_y = target_y + target->dy * max_travel_time;
                    
                    // 重新计算调整后的方向
                    int adjusted_dx = adjusted_target_x - player_center_x;
                    int adjusted_dy = adjusted_target_y - player_center_y;
                    
                    // 根据调整后的方向选择发射方向
                    if (abs(adjusted_dx) > abs(adjusted_dy)) {
                        // 水平距离更大，选择左右发射
                        shoot_direction = 0;
                    } else {
                        // 垂直距离更大，选择上下发射
                        shoot_direction = 1;
                    }
                    
                    shoot_bullets(shoot_direction);
                    
                    // 减少连发概率，更注重单发质量
                    if (active_bullet_count < MAX_BULLETS - 2 && demo_surrounded && SYS_Random() % 5 == 0) {
                        demo_shoot_timer = -1; // 设置为-1，实现快速连发（仅在被包围时）
                    }
                    
                    // 移除随机额外射击，更注重瞄准质量
                }
            }
        } else {
            // 目标太远，重置瞄准
            demo_aim_timer = 0;
            demo_aimed_shot = 0;
        }
    } else {
        // 没有目标时，寻找新目标
        if (active_enemy_count > 0) {
            // 寻找最近的敌人
            int closest_enemy = -1;
            int closest_distance = 9999;
            int i;
            
            for (i = 0; i < MAX_ENEMIES; i++) {
                if (enemies[i].active) {
                    // 计算从玩家中心到敌人中心的距离
                    int enemy_center_x = enemies[i].sprite.screen_x + enemies[i].sprite.w / 2;
                    int enemy_center_y = enemies[i].sprite.screen_y + enemies[i].sprite.h / 2;
                    
                    int dx = abs(enemy_center_x - (playerSprite.screen_x + playerSprite.w / 2));
                    int dy = abs(enemy_center_y - (playerSprite.screen_y + playerSprite.h / 2));
                    int distance = dx + dy;
                    
                    if (distance < closest_distance) {
                        closest_distance = distance;
                        closest_enemy = i;
                    }
                }
            }
            
            if (closest_enemy != -1 && closest_distance < 60) {
                // 设置新目标
                demo_target_enemy = closest_enemy;
                demo_aim_timer = 0;
                demo_aimed_shot = 0;
            }
        } else {
            // 没有敌人时，重置瞄准
            demo_aim_timer = 0;
            demo_aimed_shot = 0;
        }
    }
}


/* 检测是否被包围 */
static int check_if_surrounded(Sprite *sprite)
{
    int nearby_enemies = 0;
    int i;
    
    // 检查四周40像素范围内是否有敌人
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            int dx = abs(enemies[i].sprite.screen_x - sprite->screen_x);
            int dy = abs(enemies[i].sprite.screen_y - sprite->screen_y);
            int distance = dx + dy; // 简化的曼哈顿距离
            
            if (distance < 40) { // 40像素范围内
                nearby_enemies++;
            }
        }
    }
    
    // 如果周围有3个或更多敌人，认为被包围
    return (nearby_enemies >= 3);
}

/* 演示模式下的自动躲避和主动攻击逻辑 */
static void demo_auto_dodge(Sprite *sprite)
{
    int i;
    
    // 检测是否被包围
    int currently_surrounded = check_if_surrounded(sprite);
    
    // 更新被包围状态
    if (currently_surrounded && !demo_surrounded) {
        // 刚刚被包围
        demo_surrounded = 1;
        demo_surrounded_timer = 0;
    } else if (!currently_surrounded && demo_surrounded) {
        // 刚刚脱离包围
        demo_surrounded = 0;
        demo_surrounded_timer = 0;
    }
    
    // 如果被包围，增加计时器
    if (demo_surrounded) {
        demo_surrounded_timer++;
    }
    
    // 增强敌人坐标获取和路径计算
    int closest_enemy = -1;
    int closest_distance = 9999;
    int most_threatening_enemy = -1; // 最具威胁的敌人
    int highest_threat_score = -9999;
    
    // 计算所有敌人的威胁评分
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            // 获取敌人精确坐标
            int enemy_center_x = enemies[i].sprite.screen_x + enemies[i].sprite.w / 2;
            int enemy_center_y = enemies[i].sprite.screen_y + enemies[i].sprite.h / 2;
            int player_center_x = sprite->screen_x + sprite->w / 2;
            int player_center_y = sprite->screen_y + sprite->h / 2;
            
            // 计算与敌人的当前距离
            int dx = abs(enemy_center_x - player_center_x);
            int dy = abs(enemy_center_y - player_center_y);
            int distance = dx + dy; // 简化的曼哈顿距离
            
            // 增强预判敌人未来位置（基于敌人移动方向和速度）
            int predict_frames = 8; // 增加预判帧数到8帧
            int predict_x = enemy_center_x + enemies[i].dx * predict_frames;
            int predict_y = enemy_center_y + enemies[i].dy * predict_frames;
            
            // 计算与预判位置的距离
            int predict_dx = abs(predict_x - player_center_x);
            int predict_dy = abs(predict_y - player_center_y);
            int predict_distance = predict_dx + predict_dy;
            
            // 使用当前距离和预判距离的加权平均值作为最终距离
            int final_distance = (distance + predict_distance * 2) / 3; // 增加预判权重
            
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
            
            // 如果预判位置更接近，威胁更大
            if (predict_distance < distance) {
                threat_score += 10;
            }
            
            // 更新最近敌人和最具威胁敌人
            if (final_distance < closest_distance) {
                closest_distance = final_distance;
                closest_enemy = i;
            }
            
            if (threat_score > highest_threat_score) {
                highest_threat_score = threat_score;
                most_threatening_enemy = i;
            }
        }
    }
    
    // 优先考虑最具威胁的敌人，但也考虑最近的敌人
    int target_enemy = (SYS_Random() % 3 == 0) ? most_threatening_enemy : closest_enemy;
    
    // 增强AI决策：根据是否被包围采取不同策略
    if (demo_surrounded) {
        // 被包围时，主动突围而不是躲避，使用斜线移动
        demo_surrounded_timer++;
        demo_diagonal_move = 1; // 被包围时启用斜线移动
        
        // 计算所有敌人的合力方向，然后朝相反方向移动
        int total_dx = 0;
        int total_dy = 0;
        int enemy_count = 0;
        
        for (i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                // 使用增强的预判位置进行方向判断
                int predict_frames = 8; // 增加预判帧数
                int enemy_center_x = enemies[i].sprite.screen_x + enemies[i].sprite.w / 2;
                int enemy_center_y = enemies[i].sprite.screen_y + enemies[i].sprite.h / 2;
                int predict_x = enemy_center_x + enemies[i].dx * predict_frames;
                int predict_y = enemy_center_y + enemies[i].dy * predict_frames;
                
                int dx = predict_x - (sprite->screen_x + sprite->w / 2);
                int dy = predict_y - (sprite->screen_y + sprite->h / 2);
                
                // 根据敌人威胁程度加权方向向量
                int weight = 1;
                if (enemies[i].move_type == 2) weight = 2; // 追击型敌人权重更高
                if (enemies[i].health > 1) weight += enemies[i].health - 1; // 血量多的敌人权重更高
                
                // 累加所有敌人的方向向量
                total_dx += dx * weight;
                total_dy += dy * weight;
                enemy_count++;
            }
        }
        
        // 计算逃离方向（敌人方向的相反方向）
        if (total_dx != 0 || total_dy != 0) {
            // 归一化并设置速度
            int length = abs(total_dx) + abs(total_dy);
            if (length > 0) {
                // 使用更智能的逃离策略
                if (abs(total_dx) > abs(total_dy) * 2) {
                    // 水平方向为主
                    demo_dx = (total_dx > 0) ? -player_speed : player_speed;
                    demo_dy = (-total_dy * player_speed) / abs(total_dx);
                } else if (abs(total_dy) > abs(total_dx) * 2) {
                    // 垂直方向为主
                    demo_dy = (total_dy > 0) ? -player_speed : player_speed;
                    demo_dx = (-total_dx * player_speed) / abs(total_dy);
                } else {
                    // 对角线方向，使用45度角逃离
                    demo_dx = (total_dx > 0) ? -player_speed : player_speed;
                    demo_dy = (total_dy > 0) ? -player_speed : player_speed;
                }
            }
        } else {
            // 如果没有明显的敌人方向，随机选择一个斜线方向
            demo_dx = (SYS_Random() % 3 - 1) * player_speed;
            demo_dy = (SYS_Random() % 3 - 1) * player_speed;
            
            // 确保至少有一个方向不为0
            if (demo_dx == 0 && demo_dy == 0) {
                demo_dx = (SYS_Random() % 2 == 0) ? player_speed : -player_speed;
            }
        }
        
        // 如果被包围超过一段时间，尝试随机方向突围
        if (demo_surrounded_timer > 20) { // 减少等待时间
            // 智能随机突围：选择敌人最少的方向
            int dir;
            int best_dir = 0;
            int min_enemies = 9999;
            int future_x, future_y, enemies_in_dir;
            int enemy_center_x, enemy_center_y, dist;
            
            for (dir = 0; dir < 4; dir++) {
                // 计算未来位置
                if (dir == 0) { // 右
                    future_x = sprite->screen_x + player_speed * 10;
                    future_y = sprite->screen_y;
                } else if (dir == 1) { // 左
                    future_x = sprite->screen_x - player_speed * 10;
                    future_y = sprite->screen_y;
                } else if (dir == 2) { // 下
                    future_x = sprite->screen_x;
                    future_y = sprite->screen_y + player_speed * 10;
                } else { // 上
                    future_x = sprite->screen_x;
                    future_y = sprite->screen_y - player_speed * 10;
                }
                
                enemies_in_dir = 0;
                
                for (i = 0; i < MAX_ENEMIES; i++) {
                    if (enemies[i].active) {
                        enemy_center_x = enemies[i].sprite.screen_x + enemies[i].sprite.w / 2;
                        enemy_center_y = enemies[i].sprite.screen_y + enemies[i].sprite.h / 2;
                        dist = abs(enemy_center_x - future_x) + abs(enemy_center_y - future_y);
                        if (dist < 30) enemies_in_dir++;
                    }
                }
                
                if (enemies_in_dir < min_enemies) {
                    min_enemies = enemies_in_dir;
                    best_dir = dir;
                }
            }
            
            // 设置移动方向
            if (best_dir == 0) { // 右
                demo_dx = player_speed;
                demo_dy = 0;
            } else if (best_dir == 1) { // 左
                demo_dx = -player_speed;
                demo_dy = 0;
            } else if (best_dir == 2) { // 下
                demo_dx = 0;
                demo_dy = player_speed;
            } else { // 上
                demo_dx = 0;
                demo_dy = -player_speed;
            }
            demo_surrounded_timer = 0; // 重置计时器
        }
    } else if (target_enemy != -1) {
        // 未被包围时的正常策略
        Enemy *enemy = &enemies[target_enemy];
        
        // 使用增强的预判位置进行决策
        int predict_frames = 8; // 增加预判帧数
        int enemy_center_x = enemy->sprite.screen_x + enemy->sprite.w / 2;
        int enemy_center_y = enemy->sprite.screen_y + enemy->sprite.h / 2;
        int player_center_x = sprite->screen_x + sprite->w / 2;
        int player_center_y = sprite->screen_y + sprite->h / 2;
        int predict_x = enemy_center_x + enemy->dx * predict_frames;
        int predict_y = enemy_center_y + enemy->dy * predict_frames;
        
        // 计算与敌人的相对位置（使用预判位置）
        int dx = predict_x - player_center_x;
        int dy = predict_y - player_center_y;
        
        // 新增：先瞄准，再移动到同一行，再发射的策略
        int aim_offset_x = abs(dx); // X方向偏移量
        int aim_offset_y = abs(dy); // Y方向偏移量
        int aim_quality = 0; // 瞄准质量评分
        
        // 计算瞄准质量：偏移量越小，质量越高
        if (aim_offset_x < 10) aim_quality += 5; // X方向偏移小于10像素
        if (aim_offset_y < 10) aim_quality += 5; // Y方向偏移小于10像素
        if (aim_offset_x < 5) aim_quality += 3; // X方向偏移小于5像素
        if (aim_offset_y < 5) aim_quality += 3; // Y方向偏移小于5像素
        
        // 根据敌人类型和血量调整策略
        int aggressive_approach = (enemy->health == 1 && enemy->move_type != 2); // 血量少且非追击型，积极攻击
        int defensive_approach = (enemy->health > 1 || enemy->move_type == 2); // 血量多或追击型，谨慎应对
        
        // 优先移动到同一行或同一列，提高瞄准质量
        int should_align_x = (aim_offset_x > aim_offset_y); // X方向偏移更大，优先对齐X
        int should_align_y = (aim_offset_y > aim_offset_x); // Y方向偏移更大，优先对齐Y
        
        if (closest_distance < 50) { // 扩大近距离范围
            // 非常接近，先瞄准，再移动到同一行，再发射
            demo_diagonal_move = 0; // 先不使用斜线移动，专注于对齐
            
            // 优先移动到同一行或同一列，提高瞄准质量
            if (should_align_x) {
                // X方向偏移更大，优先对齐X（移动到同一列）
                if (dx > 0) {
                    demo_dx = player_speed; // 向右移动
                } else {
                    demo_dx = -player_speed; // 向左移动
                }
                demo_dy = 0; // 暂时不垂直移动
                
                // 如果已经对齐X，开始微调Y
                if (aim_offset_x < 15) {
                    if (dy > 0) {
                        demo_dy = player_speed / 2; // 向下微调
                    } else {
                        demo_dy = -player_speed / 2; // 向上微调
                    }
                }
            } else if (should_align_y) {
                // Y方向偏移更大，优先对齐Y（移动到同一行）
                if (dy > 0) {
                    demo_dy = player_speed; // 向下移动
                } else {
                    demo_dy = -player_speed; // 向上移动
                }
                demo_dx = 0; // 暂时不水平移动
                
                // 如果已经对齐Y，开始微调X
                if (aim_offset_y < 15) {
                    if (dx > 0) {
                        demo_dx = player_speed / 2; // 向右微调
                    } else {
                        demo_dx = -player_speed / 2; // 向左微调
                    }
                }
            }
            
            // 如果已经对齐得很好，可以开始斜线移动以躲避
            if (aim_offset_x < 10 && aim_offset_y < 10) {
                demo_diagonal_move = 1; // 启用斜线移动以躲避
                
                // 根据敌人类型选择策略
                if (aggressive_approach) {
                    // 积极攻击：保持对齐，准备射击
                    // 不改变移动方向，保持当前对齐状态
                } else {
                    // 谨慎应对：移动到敌人的射击盲区或侧翼
                    // 计算敌人的移动方向，然后移动到其侧翼
                    int enemy_move_angle = 0;
                    if (enemy->dx != 0 || enemy->dy != 0) {
                        // 计算敌人移动方向的角度（简化）
                        if (abs(enemy->dx) > abs(enemy->dy)) {
                            enemy_move_angle = (enemy->dx > 0) ? 0 : 180; // 右或左
                        } else {
                            enemy_move_angle = (enemy->dy > 0) ? 90 : 270; // 下或上
                        }
                    }
                    
                    // 移动到敌人移动方向的垂直方向（侧翼）
                    int flank_angle = (enemy_move_angle + 90) % 360;
                    if (SYS_Random() % 2 == 0) {
                        flank_angle = (enemy_move_angle - 90 + 360) % 360; // 另一侧
                    }
                    
                    // 根据侧翼角度设置移动方向
                    if (flank_angle < 45 || flank_angle >= 315) {
                        demo_dx = player_speed; // 右
                        demo_dy = 0;
                    } else if (flank_angle < 135) {
                        demo_dx = 0;
                        demo_dy = player_speed; // 下
                    } else if (flank_angle < 225) {
                        demo_dx = -player_speed; // 左
                        demo_dy = 0;
                    } else {
                        demo_dx = 0;
                        demo_dy = -player_speed; // 上
                    }
                }
            }
            
            // AI学习：根据瞄准质量给予奖励
            if (demo_mode && ai_learning_enabled && last_behavior_type >= 0) {
                if (aim_quality >= 8) {
                    behavior_scores[last_behavior_type] += good_aim_bonus; // 高质量瞄准奖励
                } else if (aim_quality >= 4) {
                    behavior_scores[last_behavior_type] += aim_offset_bonus; // 中等质量瞄准奖励
                }
                behavior_attempts[last_behavior_type]++;
            }
            
            // 确保不会后退到角落
            int next_x = sprite->screen_x + demo_dx;
            int next_y = sprite->screen_y + demo_dy;
            
            // 检查是否会进入角落区域
            if ((next_x < CORNER_SIZE && next_y < CORNER_SIZE) || // 左上角
                (next_x > WIDTH - CORNER_SIZE && next_y < CORNER_SIZE) || // 右上角
                (next_x < CORNER_SIZE && next_y > HEIGHT - CORNER_SIZE) || // 左下角
                (next_x > WIDTH - CORNER_SIZE && next_y > HEIGHT - CORNER_SIZE)) { // 右下角
                
                // 如果会进入角落，调整方向远离角落
                if (next_x < CORNER_SIZE) {
                    demo_dx = player_speed; // 向右
                } else if (next_x > WIDTH - CORNER_SIZE) {
                    demo_dx = -player_speed; // 向左
                }
                
                if (next_y < CORNER_SIZE) {
                    demo_dy = player_speed; // 向下
                } else if (next_y > HEIGHT - CORNER_SIZE) {
                    demo_dy = -player_speed; // 向上
                }
            }
        } else if (closest_distance < 60) {
            // 中等距离，根据敌人血量和移动类型决定攻击或躲避
            if (enemy->health > 1 || enemy->move_type == 2) {
                // 敌人血量多或者是追击型，倾向于躲避，使用斜线移动
                demo_diagonal_move = 1;
                
                if (abs(dx) > abs(dy)) {
                    demo_dx = (dx > 0) ? -player_speed : player_speed;
                    demo_dy = (-dy * player_speed) / abs(dx);
                } else {
                    demo_dy = (dy > 0) ? -player_speed : player_speed;
                    demo_dx = (-dx * player_speed) / abs(dy);
                }
            } else {
                // 敌人血量少且不是追击型，倾向于主动攻击，使用斜线移动
                demo_diagonal_move = 1;
                
                if (abs(dx) > abs(dy)) {
                    demo_dx = (dx > 0) ? player_speed : -player_speed;
                    demo_dy = (dy * player_speed) / abs(dx);
                } else {
                    demo_dy = (dy > 0) ? player_speed : -player_speed;
                    demo_dx = (dx * player_speed) / abs(dy);
                }
            }
        } else {
            // 距离较远时不改变方向，保持当前移动策略
            // 如果当前不是斜线移动，有概率切换到斜线移动
            if (!demo_diagonal_move && SYS_Random() % 10 == 0) {
                demo_diagonal_move = 1;
                demo_dx = (SYS_Random() % 3 - 1) * player_speed;
                demo_dy = (SYS_Random() % 3 - 1) * player_speed;
                
                // 确保至少有一个方向不为0
                if (demo_dx == 0 && demo_dy == 0) {
                    demo_dx = (SYS_Random() % 2 == 0) ? player_speed : -player_speed;
                }
            }
        }
    }
}

/* 执行按键动作 - 恢复大小调整功能，使用相对屏幕坐标 */
static int execute_key_action(Sprite *sprite)
{
	int need_redraw = 0;
	
	// 处理Start键切换演示模式
	static int prev_start_state = 1; // 初始为未按下状态
	int current_start_state = (buttonState & KEY_START);
	
	// 检测Start键的按下（从释放到按下）
	if (prev_start_state != 0 && current_start_state == 0) {
		// Start键被按下，切换演示模式
		demo_mode = !demo_mode;
		// 移除进入演示模式时的变量重置，实现演示模式持久化
		need_redraw = 1;
	}
	prev_start_state = current_start_state;
	
	// 如果在演示模式下，执行自动移动、攻击和躲避
	if (demo_mode) {
		demo_auto_dodge(sprite);
		demo_auto_move(sprite);
		demo_auto_shoot();
		return 1; // 演示模式下总是需要重绘
	}
	
	// 非演示模式下的正常按键处理
	// 简化移动延迟计算
	int move_delay = (sprite->w + sprite->h) / 20;
	if (move_delay < 1) move_delay = 1;
	if (move_delay > 3) move_delay = 3;
	
	// 更新子弹发射计时器
	if (player_shoot_timer > 0) {
		player_shoot_timer--;
	}
	
	// 只有当帧计数器达到移动延迟时才处理移动和射击
	if (frame_counter % move_delay == 0) {
		// A键发射左右子弹 - 每3帧发射一次
		if(!(buttonState & KEY_A) && player_shoot_timer == 0) {
			shoot_bullets(0);  // 0表示左右发射
			player_shoot_timer = player_shoot_interval; // 重置发射计时器
		}
		
		// B键发射上下子弹 - 在滚动时禁止发射纵向子弹，每3帧发射一次
		if(!(buttonState & KEY_B) && scrollDirection == 0 && player_shoot_timer == 0) {
			shoot_bullets(1);  // 1表示上下发射
			player_shoot_timer = player_shoot_interval; // 重置发射计时器
		}

		// 检查每个按键是否被按下
		if(!(buttonState & KEY_LEFT)) {
			if (sprite->screen_x > 10) {
				sprite->screen_x -= player_speed;  // 更新屏幕相对位置，使用player_speed
				// 确保不会越过边界
				if (sprite->screen_x < 10) sprite->screen_x = 10;
				need_redraw = 1;
			} else {
				// 玩家不再控制滚动
			}
		}
		if(!(buttonState & KEY_RIGHT)) {
			if (sprite->screen_x < 150 - sprite->w) {
				sprite->screen_x += player_speed;  // 更新屏幕相对位置，使用player_speed
				// 确保不会越过边界
				if (sprite->screen_x > 150 - sprite->w) sprite->screen_x = 150 - sprite->w;
				need_redraw = 1;
			} else {
				// 玩家不再控制滚动
			}
		}
		if(!(buttonState & KEY_UP)) {
			if (sprite->screen_y > 10) {
				sprite->screen_y -= player_speed;  // 更新屏幕相对位置，使用player_speed
				// 确保不会越过边界
				if (sprite->screen_y < 10) sprite->screen_y = 10;
				need_redraw = 1;
			}
		}
		if(!(buttonState & KEY_DOWN)) {
			if (sprite->screen_y < 110 - sprite->h) {
				sprite->screen_y += player_speed;  // 更新屏幕相对位置，使用player_speed
				// 确保不会越过边界
				if (sprite->screen_y > 110 - sprite->h) sprite->screen_y = 110 - sprite->h;
				need_redraw = 1;
			}
		}
	}

	// 玩家不再控制滚动，所以这里不需要停止滚动
	// if ((buttonState & KEY_LEFT) && (buttonState & KEY_RIGHT)) {
	// 	scrollDirection = 0;
	// 	need_redraw = 1;
	// }

	
	// 颜色调整 - 使用KEY_START键，但在演示模式下禁用
	// 注意：这里使用prev_start_state来检测按键释放，避免与上面的切换演示模式逻辑冲突
	if(prev_start_state != 0 && current_start_state == 0 && !demo_mode) {
		// Start键被按下且不在演示模式下，改变颜色
		sprite->color = (sprite->color + 1) & 0x3F;
		need_redraw = 1;
	}
	
	// SELECT键不做任何事
	// if(!(buttonState & KEY_SELECT)) {
	// 	// 不执行任何操作
	// }
	
	// 如果需要重绘，将屏幕相对坐标转换为真实坐标
	if (need_redraw) {
		screen_to_real_coords(sprite);
	}
	
	return need_redraw;
}


int main()
{
	int i;
	
	// 预计算screenMemory中每一行的地址高字节
	byte screenMemoryRowHighBytes[HEIGHT];
	for (i=0; i<HEIGHT; i++) {
		screenMemoryRowHighBytes[i] = ((word)screenMemory + i * 256) >> 8;
	}
	
	init_sprite(&playerSprite, WIDTH / 2 - 3, HEIGHT / 2 - 3, 6, 6, 0x3F);
	
	// 初始化子弹系统
	init_bullets();
	
	// 初始化敌人系统
	init_enemies();
	
	sound_reset(0);
	SYS_SetMode(2);
	bg();
	// 初始绘制精灵 - 使用精灵指令
	draw_sprite(&playerSprite);
	prevPlayerSprite = playerSprite;
	videoTop_v5 = HEIGHT;  // Safe no-op on ROMs without videoTop
	videoTop_v5 = 0;  // 显示全部

	while(1) {
		byte nextFrame = frameCount + 1;
		while (frameCount != nextFrame) /**/ ;
		
		frame_counter++; // 更新帧计数器
		
		prevPlayerSprite = playerSprite;
		// 处理按键
		int need_redraw = execute_key_action(&playerSprite);
		
		// 更新子弹位置
		update_bullets();
		
		// 更新敌人位置
		update_enemies();
		
		// 检查子弹和敌人的碰撞
		check_bullet_enemy_collisions();
		
		// 检查玩家和敌人的碰撞
		int collision = check_player_enemy_collision();
		
		// 如果玩家失去所有生命，重置游戏
		if (player_lives <= 0) {
			reset_game();
			continue; // 跳过本帧的其余部分
		}
		
		// 生成新敌人
		enemy_spawn_timer++;
		if (!auto_scroll_active && enemy_spawn_timer >= (30 / enemy_spawn_frequency_divisor)) { // 根据频率除数调整生成频率，自动滚动时禁止生成敌人
			enemy_spawn_timer = 0;
			// 确保屏幕上至少有1个活动敌人，并根据难度指数和滚动增加活动敌人数量
			if (active_enemy_count < (max_active_enemies + enemy_spawn_max_count_increase) || active_enemy_count < 1) {
				spawn_enemy();
			}
		}
		
		// 自动滚动逻辑
		if (active_enemy_count == 0 && !auto_scroll_active && !auto_scroll_preparing) {
			// 所有敌人被击杀，准备自动滚动
			auto_scroll_preparing = 1;
			shooting_disabled = 1; // 禁止发射子弹
		}
		
		// 检查是否可以开始自动滚动（所有子弹已销毁）
		if (auto_scroll_preparing && active_bullet_count == 0) {
			// 开始自动滚动
			auto_scroll_preparing = 0;
			auto_scroll_active = 1;
			auto_scroll_target_distance = 10; // 滚动10像素
			auto_scroll_current_distance = 0;
			auto_scroll_direction = 1; // 默认向右滚动
			scrollDirection = auto_scroll_direction; // 设置主滚动方向
		}

		if (auto_scroll_active) {
			int prev_scrollX = scrollX;
			scrollX += auto_scroll_direction; // 自动滚动速度为1像素/帧
			auto_scroll_current_distance++;

			if (scrollX < 0) scrollX = MAX_SCROLL; // 循环到MAX_SCROLL
			if (scrollX > MAX_SCROLL) scrollX = 0; // 循环到0

			// 更新总滚动距离
			total_scrolled_distance += abs(scrollX - prev_scrollX);
			
			// 每滚动20像素，敌人生成频率减半
			if (total_scrolled_distance >= 20 * enemy_spawn_frequency_divisor) {
				enemy_spawn_frequency_divisor *= 2;
			}
			
			// 每滚动40像素，敌人生成最大数量加1
			if (total_scrolled_distance >= 40 * (enemy_spawn_max_count_increase + 1)) {
				enemy_spawn_max_count_increase++;
			}

			if (auto_scroll_current_distance >= auto_scroll_target_distance) {
				// 达到目标距离，停止自动滚动
				auto_scroll_active = 0;
				auto_scroll_current_distance = 0;
				scrollDirection = 0; // 停止主滚动方向
				shooting_disabled = 0; // 恢复发射子弹功能
			}
		} else {
			// 如果没有自动滚动，确保scrollDirection为0
			scrollDirection = 0;
		}
		
		// 每帧都更新精灵的真实坐标（即使没有按键操作）
		// 这样可以确保精灵在滚动时保持正确的屏幕相对位置
		screen_to_real_coords(&playerSprite);
		
		// 总是需要重绘，因为子弹可能会移动
		// 即使need_redraw为0，也需要更新屏幕
		// 检测是否同时按下了多个方向键
		int multi_keys_pressed = 0;
		if (!(buttonState & KEY_LEFT) && !(buttonState & KEY_UP)) multi_keys_pressed = 1;
		if (!(buttonState & KEY_LEFT) && !(buttonState & KEY_DOWN)) multi_keys_pressed = 1;
		if (!(buttonState & KEY_RIGHT) && !(buttonState & KEY_UP)) multi_keys_pressed = 1;
		if (!(buttonState & KEY_RIGHT) && !(buttonState & KEY_DOWN)) multi_keys_pressed = 1;
		
		// 根据情况选择不同的绘制策略
		if (scrollDirection != 0 && multi_keys_pressed) {
			// 滚动并且按下2键：采用blit_test165.c方案，直接完全擦除，跳过复杂计算
			// 1. 先恢复背景，确保旧位置被正确清除，使用简单恢复方法
			restore_sprite_background(&prevPlayerSprite, &playerSprite, 1);
			
			// 2. 更新ShiftControl寄存器，实现硬件加速水平滚动
			// 参考h_scroll121.c，使用模运算实现循环滚动
			// 这里的(GRID_SIZE*1)是为了匹配扩展背景的宽度
			*ShiftControl = scrollX % (GRID_SIZE * 1);
			
			// 3. 然后绘制精灵到新位置
			draw_sprite(&playerSprite);
			
			// 4. 绘制所有子弹
			draw_bullets();
			
			// 5. 绘制所有敌人
			draw_enemies();
		} else {
			// 不滚动或滚动时单向运动：采用blit_test164.c方案，使用精细恢复
			// 1. 更新ShiftControl寄存器，实现硬件加速水平滚动
			// 参考h_scroll121.c，使用模运算实现循环滚动
			// 这里的(GRID_SIZE*1)是为了匹配扩展背景的宽度
			*ShiftControl = scrollX % (GRID_SIZE * 1);
			
			// 2. 先绘制精灵到新位置
			draw_sprite(&playerSprite);
			
			// 3. 然后进行恢复背景，使用精细恢复方法
			restore_sprite_background(&prevPlayerSprite, &playerSprite, 0);
			
			// 4. 绘制所有子弹
			draw_bullets();
			
			// 5. 绘制所有敌人
			draw_enemies();
		}
	}
	
	return 0;
}
