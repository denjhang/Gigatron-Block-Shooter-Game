# blit_test166.c Function Documentation

## Overview

blit_test166.c is a shooting game developed for the Gigatron platform, featuring complex enemy AI, intelligent demo mode, and adaptive difficulty system. The player controls a square block, battles various enemies on a grid background, and supports four-directional movement and four-directional shooting.

## Main Features

### 1. Basic Game Mechanics

- **Player Control**: Supports four-directional movement (up, down, left, right), initial speed of 3 pixels/frame
- **Shooting System**:
  - A button: Shoot bullets left and right simultaneously
  - B button: Shoot bullets up and down simultaneously
  - Bullet speed: 10 pixels/frame
  - Maximum simultaneous bullets: 4
  - Shooting interval: 3 frames

### 2. Enemy System

- **Enemy Types**: Distinguished by color, different colors have different health points
  - Yellow (0x3E): 3 health points
  - Orange (0x3C): 2 health points
  - Green (0x3A), Cyan (0x38), Blue (0x36): 1 health point

- **Enemy Behavior Patterns**:
  - Linear movement: Fixed direction movement
  - Random curve movement: Changes direction periodically
  - Player pursuit: Calculates direction toward player and moves

- **Enemy Spawning**:
  - Spawns from screen edges, avoiding 20x20 corner areas
  - Initially moves toward screen interior
  - Must move at least 20 pixels before leaving screen

### 3. Intelligent Demo Mode

- **AI Learning System**:
  - 8 behavior types (stationary, four-directional movement, four-directional diagonal movement)
  - Behavior scoring system based on hit rate, movement efficiency, etc.
  - Adaptive behavior selection, prioritizes high-scoring behaviors

- **Intelligent Aiming**:
  - Predicts enemy future positions (5-8 frames)
  - Calculates bullet flight time, adjusts aiming point
  - Prioritizes attacking low-health, close-range enemies

- **Intelligent Dodging**:
  - Detects surrounded state (3 or more enemies within 40 pixels)
  - When surrounded, calculates resultant force direction, escapes in opposite direction
  - Intelligently selects escape direction, chooses direction with fewest enemies

## AI Scoring Learning System Detailed Explanation

### 1. System Architecture

The AI scoring learning system is the core of the demo mode. It gradually optimizes AI's game strategy by continuously trying and evaluating the effects of different behaviors. The system mainly consists of the following parts:

- **Behavior Type Definition**: 8 different movement and shooting behaviors
- **Scoring Mechanism**: Scores each behavior based on game results
- **Behavior Selection Algorithm**: Selects optimal behavior based on scores
- **Learning Feedback**: Updates scores based on shooting results

### 2. Behavior Types Detailed

The system defines 8 basic behavior types, each with specific movement patterns and shooting strategies:

| Behavior ID | Behavior Name | Movement Direction | Diagonal Movement | Shoot While Moving | Description |
|------------|--------------|-------------------|-------------------|-------------------|-------------|
| 0 | Stationary | None | No | No | Stay in place, cannot shoot without moving |
| 1 | Move Right | Right | No | Yes | Horizontal right movement |
| 2 | Move Left | Left | No | Yes | Horizontal left movement |
| 3 | Move Down | Down | No | Yes | Vertical downward movement |
| 4 | Move Up | Up | No | Yes | Vertical upward movement |
| 5 | Move Up-Right Diagonal | Up-Right | Yes | Yes | Diagonal movement up and right |
| 6 | Move Down-Right Diagonal | Down-Right | Yes | Yes | Diagonal movement down and right |
| 7 | Move Up-Left Diagonal | Up-Left | Yes | Yes | Diagonal movement up and left |
| Default | Move Down-Left Diagonal | Down-Left | Yes | Yes | Diagonal movement down and left |

### 3. Scoring Mechanism

The scoring mechanism is the core of AI learning. It scores different behaviors based on various events in the game:

#### 3.1 Scoring Parameters

```c
static int hit_reward = 15;              // Reward score for hitting enemies
static int miss_penalty = 3;              // Penalty score for missing enemies
static int random_shoot_bonus = 5;        // Extra reward score for random shooting
static int shoot_frequency_bonus = 3;      // Extra reward score for high-frequency shooting
static int aim_offset_bonus = 10;         // Reward score for aim offset
static int good_aim_bonus = 8;           // Reward score for good aiming
```

#### 3.2 Scoring Trigger Conditions

1. **Hitting Enemies**:
   - Base reward: +15 points
   - Random shooting extra reward: 33% chance +5 points
   - Behavior attempt count +1

2. **Missing Enemies**:
   - Base penalty: -3 points
   - Behavior attempt count +1

3. **Shooting Frequency**:
   - Each shot: +3 points
   - Long time without shooting (1 second): -2 points

4. **Aiming Quality**:
   - High-quality aiming (offset <5 pixels): +8 points
   - Medium-quality aiming (offset <10 pixels): +10 points

### 4. Behavior Selection Algorithm

The behavior selection algorithm comprehensively considers scoring, exploration, and randomness:

#### 4.1 Score Calculation

```c
// Calculate average score (total score / attempt count)
int avg_score = 0;
if (behavior_attempts[i] > 0) {
    avg_score = behavior_scores[i] / behavior_attempts[i];
}
```

#### 4.2 Exploration Reward

To encourage trying new behaviors, the system provides extra rewards for behaviors with few attempts:

```c
// Significantly increase exploration opportunities, encourage random behaviors
if (behavior_attempts[i] < 5) {
    avg_score += 10; // Add exploration reward
}
```

#### 4.3 Randomness

To increase AI unpredictability and adaptability, the system introduces random factors:

```c
// Randomly give extra rewards, increase randomness
if (SYS_Random() % 4 == 0) {
    avg_score += 8; // 25% chance to get extra reward
}

// 20% chance to completely randomly select behavior, increase randomness
if (SYS_Random() % 5 == 0) {
    best_behavior = SYS_Random() % MAX_BEHAVIOR_TYPES;
}
```

#### 4.4 Movement Behavior Reward

The system specifically encourages behaviors that combine movement and shooting:

```c
// Special reward for behaviors that combine movement and shooting
if (i >= 1 && i <= 7) { // All movement behaviors
    avg_score += 3; // Extra reward for movement behaviors
}
```

### 5. Learning Feedback Mechanism

The learning feedback mechanism updates scores in real-time based on game results:

#### 5.1 Hit Feedback

When a bullet hits an enemy, the system will:

1. Increase the score of the current behavior
2. Reset consecutive miss count
3. Record shooting result as "hit"
4. Give random shooting extra reward (33% chance)

#### 5.2 Miss Feedback

When a shot misses, the system will:

1. Decrease the score of the current behavior
2. Increase consecutive miss count
3. Record shooting result as "miss"

#### 5.3 Consecutive Miss Handling

When consecutive misses are too many, the AI will adjust strategy:

```c
// AI learning: decide whether to move before shooting based on consecutive miss count
int should_move_before_shoot = (consecutive_misses >= 2);
```

### 6. Advanced AI Features

#### 6.1 Enemy Prediction

AI not only aims at the enemy's current position but also predicts enemy future positions:

```c
// Predict enemy future position (based on enemy movement direction and speed)
int predict_frames = 5; // Predict position 5 frames later
int predict_x = enemies[i].sprite.screen_x + enemies[i].dx * predict_frames;
int predict_y = enemies[i].sprite.screen_y + enemies[i].dy * predict_frames;
```

#### 6.2 Bullet Flight Time Calculation

AI calculates the time required for bullets to reach the target position and adjusts the aiming point:

```c
// Calculate time required for bullets to reach target position
int bullet_speed = 10; // Bullet speed is 10 pixels/frame
int travel_time_x = abs(dx) / bullet_speed;
int travel_time_y = abs(dy) / bullet_speed;
int max_travel_time = (travel_time_x > travel_time_y) ? travel_time_x : travel_time_y;

// Consider enemy movement, adjust aiming point
int adjusted_target_x = target_x + target->dx * max_travel_time;
int adjusted_target_y = target_y + target->dy * max_travel_time;
```

#### 6.3 Threat Assessment

AI evaluates the threat level of each enemy, prioritizing high-threat targets:

```c
// Calculate threat score
int threat_score = 0;

// Closer distance means greater threat
threat_score += (100 - distance);

// Faster enemy speed means greater threat
int enemy_speed = abs(enemies[i].dx) + abs(enemies[i].dy);
threat_score += enemy_speed * 5;

// Pursuit-type enemies are more threatening
if (enemies[i].move_type == 2) {
    threat_score += 20;
}

// Enemies with more health are more threatening
threat_score += enemies[i].health * 5;

// If enemy is approaching player, threat is greater
int approach_vector = (enemy_center_x - player_center_x) * enemies[i].dx +
                    (enemy_center_y - player_center_y) * enemies[i].dy;
if (approach_vector < 0) { // Negative dot product indicates approaching
    threat_score += 15;
}
```

### 7. Learning Effects

Through the AI scoring learning system, the AI in demo mode can:

1. **Gradually optimize behavior selection**: Prioritize using high-scoring behaviors
2. **Adapt to different game situations**: Adjust strategies based on enemy types and quantities
3. **Improve shooting accuracy**: Increase hit rate through prediction and aiming quality scoring
4. **Balance exploration and exploitation**: Maintain balance between trying new behaviors and using known good behaviors
5. **Dynamically adjust strategies**: Adjust shooting strategies based on consecutive miss count

This AI scoring learning system makes the demo mode not just a simple preset behavior sequence, but an intelligent system capable of learning and adaptation, greatly enhancing the game's viewing value and technical demonstration value.

### 4. Difficulty System

- **Dynamic Difficulty Adjustment**:
  - Every 5 enemies killed, difficulty increases by 1 (maximum level 5)
  - Difficulty increase increases maximum active enemy count (maximum 7)
  - Enemy minimum speed increases with player hit count

- **Scrolling System**:
  - Auto-scrolling triggers after all enemies are killed
  - Shooting is disabled during scrolling
  - Scroll distance affects enemy spawn frequency and quantity

### 5. Life Value and Reward System

- **Player Life Value**: Initial 3 lives
- **Life Loss**: Lose 1 life when colliding with enemies
- **Reward Mechanism**:
  - Every 5 enemies killed adds 1 life and restores color
  - Every 2 enemies killed increases movement speed by 1 (maximum 7)
  - Player color changes with life value: White → Light Blue → Red

### 6. Background and Visual Effects

- **Grid Background**: 10x10 pixel grid, blue background with green horizontal lines and cyan vertical lines
- **Sprite System**: Uses 6x6 pixel sprites, supports multi-sprite stitching
- **Background Restoration**: Intelligently restores background after sprite movement, supports both fine and simple restoration modes

## Data Structures

### 1. Sprite Structure

```c
typedef struct {
    int screen_x, screen_y; // Sprite's relative position on screen (unaffected by scrolling)
    int x, y;             // Sprite's actual position in video memory (affected by scrolling)
    int w, h;             // Sprite's width and height
    int color;            // Sprite's color
    int init_x, init_y;   // Sprite's initial position
    int width_units;       // Width unit count (1-4, each unit 6 pixels)
    int height_units;      // Height unit count (1-4, each unit 6 pixels)
} Sprite;
```

### 2. Bullet Structure

```c
typedef struct {
    Sprite sprite;   // Bullet sprite
    int dx, dy;     // Bullet movement direction and speed
    int active;      // Whether bullet is active
} Bullet;
```

### 3. Enemy Structure

```c
typedef struct {
    Sprite sprite;           // Enemy sprite
    int world_x, world_y;   // Enemy's absolute coordinates in game world
    int dx, dy;             // Enemy movement direction and speed
    int active;             // Whether enemy is active
    int health;             // Enemy health points
    int move_type;          // Movement type: 0=linear, 1=random curve, 2=pursue player
    int speed;              // Movement speed (1-3)
    int curve_counter;      // Counter for random curve movement
    int distance_traveled;  // Distance enemy has moved
    int original_color;     // Enemy's original color
    int hit_color_index;    // Color phase index after being hit
    int state;              // Enemy state: 0=entering, 1=active
    int stationary_timer;    // Stationary timer, records time enemy has been stationary (frame count)
    int prev_x, prev_y;     // Previous frame position, used to detect if stationary
} Enemy;
```

## Core Algorithms

### 1. Coordinate Transformation Algorithm

```c
static void screen_to_real_coords(Sprite *sprite)
{
    // Real X coordinate = Screen X coordinate + Scroll offset
    sprite->x = sprite->screen_x + (scrollX % GRID_SIZE);
    
    // If real X coordinate exceeds video memory range, perform circular processing
    if (sprite->x < 0) sprite->x += MAX_SCROLL + 1;
    if (sprite->x > MAX_SCROLL) sprite->x -= MAX_SCROLL + 1;
    
    // Y coordinate is not affected by horizontal scrolling
    sprite->y = sprite->screen_y;
}
```

### 2. Background Restoration Algorithm

The program implements two background restoration algorithms:

1. **Fine Restoration**: Calculates the intersection of new and old sprite positions, only restores non-overlapping parts
2. **Simple Restoration**: Completely restores the old sprite area, suitable for sprites crossing video memory boundaries

### 3. AI Behavior Selection Algorithm

```c
static int select_best_behavior()
{
    int best_behavior = 0;
    int best_score = -9999;
    int i;
    
    // Traverse all behavior types, select the one with highest score
    for (i = 0; i < MAX_BEHAVIOR_TYPES; i++) {
        // Calculate average score (total score / attempt count)
        int avg_score = 0;
        if (behavior_attempts[i] > 0) {
            avg_score = behavior_scores[i] / behavior_attempts[i];
        }
        
        // Significantly increase exploration opportunities, encourage random behaviors
        if (behavior_attempts[i] < 5) {
            avg_score += 10; // Add exploration reward
        }
        
        // Randomly give extra rewards, increase randomness
        if (SYS_Random() % 4 == 0) {
            avg_score += 8; // 25% chance to get extra reward
        }
        
        // Special reward for behaviors that combine movement and shooting
        if (i >= 1 && i <= 7) { // All movement behaviors
            avg_score += 3; // Extra reward for movement behaviors
        }
        
        if (avg_score > best_score) {
            best_score = avg_score;
            best_behavior = i;
        }
    }
    
    // 20% chance to completely randomly select behavior, increase randomness
    if (SYS_Random() % 5 == 0) {
        best_behavior = SYS_Random() % MAX_BEHAVIOR_TYPES;
    }
    
    return best_behavior;
}
```

### 4. Enemy Prediction Algorithm

```c
// Predict enemy future position (based on enemy movement direction and speed)
int predict_frames = 5; // Predict position 5 frames later
int predict_x = enemies[i].sprite.screen_x + enemies[i].dx * predict_frames;
int predict_y = enemies[i].sprite.screen_y + enemies[i].dy * predict_frames;

// Calculate time required for bullets to reach target position
int bullet_speed = 10; // Bullet speed is 10 pixels/frame
int travel_time_x = abs(dx) / bullet_speed;
int travel_time_y = abs(dy) / bullet_speed;
int max_travel_time = (travel_time_x > travel_time_y) ? travel_time_x : travel_time_y;

// Consider enemy movement, adjust aiming point
int adjusted_target_x = target_x + target->dx * max_travel_time;
int adjusted_target_y = target_y + target->dy * max_travel_time;
```

## Control Instructions

### Normal Mode Controls

- **Direction Keys**: Move up, down, left, right
- **A Button**: Shoot bullets left and right
- **B Button**: Shoot bullets up and down (disabled during scrolling)
- **Start Button**: Change player color / Switch to demo mode
- **Select Button**: No function

### Demo Mode

- Press Start button to switch to demo mode
- In demo mode, AI automatically controls player movement and shooting
- AI will learn and optimize its behavior strategies

## Technical Features

1. **Efficient Rendering**: Uses Gigatron's sprite system and hardware scrolling
2. **Intelligent Background Restoration**: Selects fine or simple restoration algorithm based on situation
3. **Adaptive AI**: AI in demo mode learns and adjusts strategies based on game situations
4. **Dynamic Difficulty**: Adjusts game difficulty based on player performance
5. **Smooth Animation**: 60FPS game loop ensures smooth gaming experience

## Compilation and Running

Use the following command to compile:
```
glcc -o blit_test166.gt1 blit_test166.c -rom=dev7 -map=64k,./blit_test141.ovl --no-runtime-bss
```

After successful compilation, the blit_test166.gt1 file is generated, which can be run on Gigatron emulator or hardware.